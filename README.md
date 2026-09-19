# IzakayaCheater

东方夜雀食堂修改器，目前可以加满好感、修改金钱、解锁全部地图、强开本体与 DLC1~DLC5 的 Boss 战。

目前支持版本：鬼知道……（反正支持4.4.0）

---

## 版权与免责声明

- 本项目是**第三方非官方**存档修改器，与《东方夜雀食堂 - Touhou Mystia's Izakaya》的开发团队、发行方无关联，也未经其授权或认可。
- 本仓库不包含、不分发任何游戏本体资源（不含游戏程序、数据包、字体、音乐、美术源文件），程序运行时也不修改游戏本体文件，只读写你自己机器上的存档。
- 文中出现的名称、商标、角色名等均为其各自权利人的资产，仅供识别之用。
- 游戏素材（包含 `web/assets/` 下的图标）著作权归游戏原作者所有，出处：[Steam - 东方夜雀食堂 - Touhou Mystia's Izakaya](https://store.steampowered.com/app/1584090/__Touhou_Mystias_Izakaya/)。
- 若权利人认为本项目内容侵犯其权益，请通过 GitHub Issue 或邮件（3178644407@qq.com）联系，我会在收到通知后尽快删除相关内容（包括但不限于下架仓库、移除素材文件、更名）。**侵权即删。**
- 本工具仅供单机存档修改与学习交流，请勿用于商业用途。使用本工具造成的任何后果（存档损坏、进度丢失、账号异常等）由使用者自行承担，请务必先备份存档。

---


## 为啥写这东西（对我个人来说）

慢慢打的话 太累了 虽然对于我来说做修改器更累了（？

---

## 注意事项（必看）

> [!CAUTION]
> **请一定一定先备份存档 然后再操作**

> [!WARNING]
> 1. 操作前请先关闭 Steam 云同步（库 → 右键游戏 → 属性 → 通用 → 取消勾选「将游戏存档保留在 Steam 云」），否则云同步会把改过的存档还原回去
> 2. 只玩过本体的存档，先用「解锁全部地图」把 DLC 地区打开，不然地图上根本去不了
> 3. 方案A「排队事件」**已停用（按钮置灰）**：只改存档 queue 实测触发不了，真正生效要在游戏运行时调用它自己的 `RunTimeScheduler.ScheduleEventExtern`，等于要改内存/注入（好心人来写写这部分功能罢）
> 4. B方式开启时 如果你的进度未到需要章节 可能会丢失未完成的进度（再次强调 一定要备份 存档丢了很心疼）
> 5. 请以**管理员身份**运行修改器

---

## 强开战斗（建议阅读方案B、C）

选存档 → 选战斗 → 点方案：

| 方案 | 写入位置 | 效果 |
|---|---|---|
| 方案A 排队事件（**已停用**，按钮置灰）| — | 只往存档写 `scheduledEvents` 实测触发不了。真正生效要在游戏运行时调用它自己的 `RunTimeScheduler.ScheduleEventExtern`，等于要改内存/注入，暂未实现 |
| 方案B 标记通关 | `finishedEvents` / `finishedMissions` | 把该战标记成已完成，解锁游戏里的「再战」入口，需要自己去对应 NPC / 地点点一下 |
| 方案C 添加邀请函 | 物品 2014~2019 | 刷好感，走原版路线（只有 DLC2 有）|

| 战斗（下拉显示名）| 方案A 事件 | 方案B 判定 | 再战入口 |
|---|---|---|---|
| 与幽幽子的决战 | `Challenge_Finale_P1` | 无判定，恒可用 | 幽幽子羁绊特殊对话 |
| 饕餮挑战赛 | `DLC1_Main_Toutetsu_First_RepeatChallenge_P1` | finishedEvents：`DLC1_Main_Toutetsu_004_Challange_Success` | 妖怪山·荷取对话（山顶） |
| 怪诞料理挑战赛 | `DLC2_Main_FormerHell_WeirdCooking_Challenge_P1` | **finishedMissions**：`DLC2_Main_FormerHell_WeirdCooking_Mission_Enter` | 地灵殿·阿燐对话（办公室门口 右转） |
| 博丽大祭 | `DLC3_Repeat_GobackHakureiShrine_Event` | **方案B**：DLC3 羁绊 400 + 39 项开关（含 `DLC3_HakureiFestival_JienYuu`）+ 祭典任务/事件 | 博丽神社·时焉侑（进去右转） |
| 芙兰朵露挑战赛 | `DLC4_Main_Part10_RepeatChallenge_Begin_Event` | finishedEvents：`DLC4_Main_FlandreCabin_Enter_Event`（芙兰登场判定，反汇编实测确认）+ 挑战完成事件，开关 `FirstTimeToSDMBasement` | **红魔馆进去直走，右侧的门**→ 地下室 → 芙兰的家，与芙兰对话 |
| 瑞灵 | `DLC5_RepeatChallenge_ArrestMizuchi_Enter_Event` | finishedEvents：`DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event` | 月都控制台（直走） |

> 注意：`scheduledEvents` 的键是**日期**（自然存档里能看到 `"62": ["Main_5_BambooForest_001_Event"]`，键 = 该事件要触发的那一天）。已试过写 `-1` 桶和写次日的键，都触发不了：`ScheduleEventExtern` 最终进的是带一堆前置校验的运行时常驻入口，光改存档队列没用。要真正通过事件触发，只能在游戏运行时调用它（注入/改内存），这个坑留给后来人。

---

## 解锁全部地图

按钮在「金钱」下面，不依赖 Boss 选择。把 `dayScenePartial.daySceneMapStatusData` 与 `dayScenePartialDLC.DLCn.daySceneMapStatusData` 里的 17 张地图全部置 `true`，并补写 `allActivatedDLC` 的 DLC1~5。适用于只玩过本体的存档（地图上根本没有 DLC 地区，没法去打 DLC 的再战）。

| 区块 | 地图 ID |
|---|---|
| 本体 `dayScenePartial` | BeastForest / HakureiShrine / HumanVillage / BambooForest / ScarletMansion / Hakugyokurou |
| DLC1 | DLC1_MagicForest / DLC1_YoukaiMountain |
| DLC2 | DLC2_FormerHell / DLC2_EarthSpiritsPalace |
| DLC3 | DLC3_MyourenTemple / DLC3_DivineSpiritMausoleum |
| DLC4 | DLC4_GardenOfTheSun / DLC4_ShiningNeedleCastle / DLC4_ScarletMansionBasement |
| DLC5 | DLC5_Makai / DLC5_LunarCapital |

返回值：`>=0` = 新解锁的地图数，`-1` 存档无此结构，`-4` 写入失败，`-5` 无法激活 DLC，`-7` 结构不支持。重复点没事（第二次返回 0 且文件不变）。

> 这只是「地图是否开放」，不改剧情进度。已验证字段写入正确。

---

## 全部满好感

按钮在「解锁全部地图」右边，不依赖 Boss 选择。把 `albumPartial.specialSkinSelection` 与 `albumPartialDLC.<DLC>.specialSkinSelection` 里**所有角色**的 `CurrentBondExp` 改成 `9999`、`CurrentBondLevel` 改成 `5`（游戏满级就是 5）。只改这两个数字，存档里其它内容一个字节不动。

返回值：`>=0` = 改动的角色数（满进度档 78 个，纯本体档 29 个），`-4` 写入失败，`-8` 该存档没有好感数据。重复点结果一样。

> DLC2/DLC3 的「满好感」只是同一批数据，所以这个按钮也能一次把 DLC2/DLC3 的条件做完。

---

# 开发数据如下

## 核心数据

```text
金钱
playerPartial.fund

邀请函 ID
2014~2019

好感（所有角色都在这两个地方）
albumPartial.specialSkinSelection.<角色ID>.CurrentBondExp / CurrentBondLevel
albumPartialDLC.<DLC>.specialSkinSelection.<角色ID>.CurrentBondExp / CurrentBondLevel
满好感 = CurrentBondExp 9999 + CurrentBondLevel 5（等级上限 5）
角色 ID 段
本体 0~29、DLC1 1000~1005、DLC2 2000~2005、DLC3 3000~3005、DLC4 4000~4005、DLC5 5000~5005

39 项开关，控制游戏内各种状态
dayScenePartial.trackedSwitch

DLC3_HakureiFestival_JienYuu: true
博丽大祭所需字段

DLC3_HakureiFestival_RepeatChallenge_JienYuuCharacter: true
博丽大祭所需字段

schedulerPartialDLC.DLC3.allTrackingMissions["0"]
任务跟踪列表，值为数组 [{...}]

schedulerPartialDLC.DLC3.finishedMissions
已完成任务列表

schedulerPartialDLC.DLC3.finishedEvents
已完成事件列表

Boss 战 A 事件（排队事件，写 schedulerPartialDLC.<DLC>.scheduledEvents，键=日期；已停用）
本体      Challenge_Finale_P1
DLC1      DLC1_Main_Toutetsu_First_RepeatChallenge_P1
DLC2      DLC2_Main_FormerHell_WeirdCooking_Challenge_P1
DLC3      DLC3_Repeat_GobackHakureiShrine_Event
DLC4      DLC4_Main_Part10_RepeatChallenge_Begin_Event
DLC5      DLC5_RepeatChallenge_ArrestMizuchi_Enter_Event

Boss 战 B 判定（写完成数组，解锁游戏里的「再战」入口，要自己去点）
本体      finishedMissions: Main_5_BambooForest_023_Mission（只标记完成，不开打）
DLC1      finishedEvents: DLC1_Main_Toutetsu_004_Challange_Success
DLC2      finishedMissions: DLC2_Main_FormerHell_WeirdCooking_Mission_Enter
DLC3      羁绊 400 + 39 项开关 + finishedEvents: DLC3_Main_Part4_Mission_Finished_Event
          + finishedMissions: DLC3_Main_Part4_KizunaProgress_Mission /
            DLC3_Main_Part4.5.3_GuidedMission / DLC3_Main_Part8_HakureiFestivalChallenge_GuidedMission
DLC4      finishedEvents: DLC4_Main_FlandreCabin_Enter_Event（她的登场判定）/
            DLC4_Main_Part10_FlandreChallenge_Finished_Event /
            DLC4_Main_Part10_FlandreChallenge_Success_GoHome_Event
          开关 FirstTimeToSDMBasement: true
DLC5      finishedEvents: DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event
          开关 DLC5_Map_Makai_Portal / DLC5_Makai_RestrictedZoneDoor: true

地图解锁（全置 true）
dayScenePartial.daySceneMapStatusData
dayScenePartialDLC.<DLC>.daySceneMapStatusData

本体      BeastForest / HakureiShrine / HumanVillage / BambooForest / ScarletMansion / Hakugyokurou
DLC1      DLC1_MagicForest / DLC1_YoukaiMountain
DLC2      DLC2_FormerHell / DLC2_EarthSpiritsPalace
DLC3      DLC3_MyourenTemple / DLC3_DivineSpiritMausoleum
DLC4      DLC4_GardenOfTheSun / DLC4_ShiningNeedleCastle / DLC4_ScarletMansionBasement
DLC5      DLC5_Makai / DLC5_LunarCapital

DLC 激活列表
allActivatedDLC: ["CORE", "DLC1", "DLC2", "DLC3", "DLC4", "DLC5"]

```

---

## 各战斗的触发点（逆向结论）

```text
本体       YuyukoExtraDialogData.challengeStartEvent
DLC1       NitoriExtraDialogData.AddRepeatChallengeItem
DLC2       OrinExtraDialogData.challengeMissionId / repeatChallengeEventId
DLC3       JienYuuBehaviourComponent.repeatChallengeEventId
DLC4       FlandreExtraDialogData.m_BeginEventLabel / m_RepeatChallengeEventId
DLC5       LunarCapitalConsoleBehaviourComponent.m_ArrestMizuchiChallengeFinishedEvent
            / m_ArrestMizuchiRepeatChallengeStartEvent

```

## 界面与运行要求

界面已经重做成网页（HTML/CSS/JS），内嵌在程序窗口里跑，不再用 wxWidgets 控件。

- 系统要求：Windows 10/11，需要 **WebView2 运行库**（Win11 自带；Win10 装了新 Edge 也有）。如果启动后窗口一片空白，装一下 [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)。
- 打开后就是界面本身，不需浏览器；关掉窗口即退出。
- 左上角「更换背景」可以选本地图片当背景，会存成 `web/assets/custom.jpg`；「恢复默认」删除它。
- 所有功能仍然只读写**本机存档**，程序不联网（本地服务只监听 127.0.0.1）。

---

## 从源码构建

需要：CMake 3.15+、支持 C++11 的编译器（MinGW-w64 或 MSVC）、wxWidgets 3.2（core + base）。

```bash
cmake -S . -B build -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DwxWidgets_ROOT_DIR=<wxWidgets 目录> \
  -DwxWidgets_LIB_DIR=<wxWidgets 目录>/lib/gcc_lib \
  -DwxWidgets_CONFIGURATION=mswu
cmake --build build -j
```

MSVC 就把 `-G` 换成对应生成器并删掉那两个 wxWidgets 路径参数（让 CMake 自己找）。

构建后 `build/` 里会有 `IzakayaCheater.exe`，同时自动拷入 `web/` 目录和 `WebView2Loader.dll`——**分发时三者必须放一起**，缺了界面会打不开。WebView2 的头文件与加载器已随仓库放在 `third_party/webview2`（取自 Microsoft.Web.WebView2 1.0.4191.47）。

---


RunTimeScheduler.ScheduleEventExtern(string) -> scheduledEvents
RunTimeScheduler.HaveEventFinished(string)   -> finishedEvents
RunTimeScheduler.HaveMissionFinished(string) -> finishedMissions
```
