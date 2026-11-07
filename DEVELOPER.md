# IzakayaCheater 开发说明

面向改代码和查字段的人 普通使用看 [README.md](README.md)

## 目录结构

```text
main.cpp            wx 窗口 WebView2 宿主 本地服务启动
webview_host.cpp    WebView2 环境 控制器 尺寸同步 DPI
web_server.cpp      本地 HTTP 服务 只监听 127.0.0.1
save_editor.cpp     存档读写 地图 好感 Boss 全部逻辑
web/                界面 index.html style.css app.js assets
third_party/        WebView2 头文件和加载器
installer/          Inno Setup 安装包脚本
.github/workflows/  Release 打包
```

## 从源码构建

需要

- CMake 3.15+
- 支持 C++11 的编译器 MinGW-w64 或 MSVC
- wxWidgets 3.2 core 和 base

MinGW 命令

```bash
cmake -S . -B build -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DwxWidgets_ROOT_DIR=<wxWidgets 目录> \
  -DwxWidgets_LIB_DIR=<wxWidgets 目录>/lib/gcc_lib \
  -DwxWidgets_CONFIGURATION=mswu
cmake --build build -j
```

MSVC 就把 `-G` 换成对应生成器 删掉两个 wxWidgets 路径参数 让 CMake 自己找

构建完 `build/` 里有 `IzakayaCheater.exe` CMake 会自动拷入 `web/` 和 `WebView2Loader.dll`

MinGW 构建还要带三个运行库才算完整

```text
libstdc++-6.dll
libgcc_s_seh-1.dll
libwinpthread-1.dll
```

分发时 exe web WebView2Loader.dll 加这三个 DLL 缺了界面打不开或直接闪退

WebView2 头文件和加载器在 `third_party/webview2` 取自 Microsoft.Web.WebView2 1.0.4191.47

## 核心数据

```text
金钱
playerPartial.fund

邀请函 ID
2014~2019

好感
albumPartial.specialSkinSelection.<角色ID>.CurrentBondExp / CurrentBondLevel
albumPartialDLC.<DLC>.specialSkinSelection.<角色ID>.CurrentBondExp / CurrentBondLevel
满好感 = CurrentBondExp 9999  CurrentBondLevel 5  等级上限就是 5

角色 ID 段
本体 0~29  DLC1 1000~1005  DLC2 2000~2005
DLC3 3000~3005  DLC4 4000~4005  DLC5 5000~5005

39 项开关
dayScenePartial.trackedSwitch

DLC3_HakureiFestival_JienYuu  true
DLC3_HakureiFestival_RepeatChallenge_JienYuuCharacter  true
博丽大祭需要这两个

schedulerPartialDLC.DLC3.allTrackingMissions["0"]  任务跟踪 值为数组
schedulerPartialDLC.DLC3.finishedMissions          已完成任务
schedulerPartialDLC.DLC3.finishedEvents            已完成事件
```

## 地图解锁

把 `dayScenePartial.daySceneMapStatusData` 和 `dayScenePartialDLC.DLCn.daySceneMapStatusData` 里 17 张地图全置 true
再补写 `allActivatedDLC` 的 DLC1~5

| 区块 | 地图 ID |
|---|---|
| 本体 | BeastForest HakureiShrine HumanVillage BambooForest ScarletMansion Hakugyokurou |
| DLC1 | DLC1_MagicForest DLC1_YoukaiMountain |
| DLC2 | DLC2_FormerHell DLC2_EarthSpiritsPalace |
| DLC3 | DLC3_MyourenTemple DLC3_DivineSpiritMausoleum |
| DLC4 | DLC4_GardenOfTheSun DLC4_ShiningNeedleCastle DLC4_ScarletMansionBasement |
| DLC5 | DLC5_Makai DLC5_LunarCapital |

```text
allActivatedDLC: ["CORE" "DLC1" "DLC2" "DLC3" "DLC4" "DLC5"]
```

返回值

```text
>=0  新解锁地图数
-1   存档没有这个结构
-4   写入失败
-5   无法激活 DLC
-7   结构不支持
```

## 强开 Boss 战

### 方案 A 排队事件 已停用

写 `schedulerPartialDLC.<DLC>.scheduledEvents` 键是日期

```text
本体   Challenge_Finale_P1
DLC1   DLC1_Main_Toutetsu_First_RepeatChallenge_P1
DLC2   DLC2_Main_FormerHell_WeirdCooking_Challenge_P1
DLC3   DLC3_Repeat_GobackHakureiShrine_Event
DLC4   DLC4_Main_Part10_RepeatChallenge_Begin_Event
DLC5   DLC5_RepeatChallenge_ArrestMizuchi_Enter_Event
```

`scheduledEvents` 的键是触发日期

```json
"62": ["Main_5_BambooForest_001_Event"]
```

试过写 -1 桶和写次日键 都触发不了
`ScheduleEventExtern` 进的是带前置校验的运行时常驻入口 光改存档队列没用
真要触发只能在游戏运行时调用 也就是注入 改内存

### 方案 B 标记通关

写完成数组 解锁游戏里的再战入口 要玩家自己去点

```text
本体   finishedMissions: Main_5_BambooForest_023_Mission  只标记完成 不开打
DLC1   finishedEvents: DLC1_Main_Toutetsu_004_Challange_Success
DLC2   finishedMissions: DLC2_Main_FormerHell_WeirdCooking_Mission_Enter
DLC3   羁绊 400 加 39 项开关 加 finishedEvents: DLC3_Main_Part4_Mission_Finished_Event
       加 finishedMissions: DLC3_Main_Part4_KizunaProgress_Mission
       DLC3_Main_Part4.5.3_GuidedMission
       DLC3_Main_Part8_HakureiFestivalChallenge_GuidedMission
DLC4   finishedEvents: DLC4_Main_FlandreCabin_Enter_Event 芙兰登场判定
       DLC4_Main_Part10_FlandreChallenge_Finished_Event
       DLC4_Main_Part10_FlandreChallenge_Success_GoHome_Event
       开关 FirstTimeToSDMBasement true
DLC5   finishedEvents: DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event
       开关 DLC5_Map_Makai_Portal  DLC5_Makai_RestrictedZoneDoor true
```

### 完整对照表

| 战斗 | 方案 A 事件 | 方案 B 判定 | 再战入口 |
|---|---|---|---|
| 与幽幽子的决战 | `Challenge_Finale_P1` | 无判定 恒可用 | 幽幽子羁绊特殊对话 |
| 饕餮挑战赛 | `DLC1_Main_Toutetsu_First_RepeatChallenge_P1` | finishedEvents `DLC1_Main_Toutetsu_004_Challange_Success` | 妖怪山 荷取 山顶 |
| 怪诞料理挑战赛 | `DLC2_Main_FormerHell_WeirdCooking_Challenge_P1` | finishedMissions `DLC2_Main_FormerHell_WeirdCooking_Mission_Enter` | 地灵殿 阿燐 |
| 博丽大祭 | `DLC3_Repeat_GobackHakureiShrine_Event` | 羁绊 400 加 39 项开关加祭典任务事件 | 博丽神社 时焉侑 |
| 芙兰朵露挑战赛 | `DLC4_Main_Part10_RepeatChallenge_Begin_Event` | finishedEvents `DLC4_Main_FlandreCabin_Enter_Event` 加挑战完成事件 开关 `FirstTimeToSDMBasement` | 红魔馆右侧门 地下室 芙兰的家 |
| 瑞灵 | `DLC5_RepeatChallenge_ArrestMizuchi_Enter_Event` | finishedEvents `DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event` | 月都控制台 |

## 各战斗触发点 逆向结论

```text
本体   YuyukoExtraDialogData.challengeStartEvent
DLC1   NitoriExtraDialogData.AddRepeatChallengeItem
DLC2   OrinExtraDialogData.challengeMissionId / repeatChallengeEventId
DLC3   JienYuuBehaviourComponent.repeatChallengeEventId
DLC4   FlandreExtraDialogData.m_BeginEventLabel / m_RepeatChallengeEventId
DLC5   LunarCapitalConsoleBehaviourComponent.m_ArrestMizuchiChallengeFinishedEvent
       / m_ArrestMizuchiRepeatChallengeStartEvent
```

## RunTimeScheduler 映射

```text
RunTimeScheduler.ScheduleEventExtern(string)  -> scheduledEvents
RunTimeScheduler.HaveEventFinished(string)    -> finishedEvents
RunTimeScheduler.HaveMissionFinished(string)  -> finishedMissions
```

## 界面和尺寸

界面是纯静态页面 由本地服务提供 WebView2 加载
窗口缩放时 `OnSize` 把 panel 设成客户区大小 再同步 WebView bounds
最大化后 WebView 只占左上角 是 panel 没跟着拉伸 不是 WebView2 的问题

跨显示器 DPI 变化会在 `OnDpiChanged` 里重算 bounds 缩放交给 WebView2 自动检测

## 发布

推 `v*` tag 会自动构建并创建 Release

Actions 里手动 dispatch 只构建并上传 artifact 不发 Release 适合先验证

产物

```text
IzakayaCheater-<tag>-win64.zip          便携包 exe web 加载器 加三个运行库
IzakayaCheater-Setup-<版本>.exe          Inno Setup 安装包
```

CI 用 MSYS2 UCRT64 加官方 wxWidgets 3.2.11 gcc1420UCRT x64 预编译静态库
静态链接 分发包不需要 wx 自己的 DLL

发版前三处版本号要一致

```text
CMakeLists.txt   project(IzakayaCheater VERSION x.y.z)
installer/installer.iss   #define MyAppVersion "x.y.z"
git tag          vx.y.z
```

## 署名

usfese 和 雾雨卢本伟

许可证 MIT 素材与商标归原权利人 见 [LICENSE](LICENSE)
