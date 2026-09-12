# IzakayaCheater

东方夜雀食堂修改器，可以修改金钱、强开本体与 DLC1~DLC5 的 Boss 战。

目前支持版本：鬼知道……（反正支持4.4.0）

---

## 为啥写这东西（对我个人来说）

DLC2 和 DLC3 的 Boss 战需要 12 角色满好感，太累了，所以写了。

---

## 注意事项（必看）

> [!CAUTION]
> **请一定一定先备份存档 然后再操作**

> [!WARNING]
> 1. 操作前请先关闭 Steam 云同步（库 → 右键游戏 → 属性 → 通用 → 取消勾选「将游戏存档保留在 Steam 云」），否则云同步会把改过的存档还原回去
> 2. 开启「博丽大祭」时 如果你的进度未到狸猫的情报大作战 可能会丢失 DLC3 未完成的进度（现在由「博丽大祭」的方案A 触发，风险相同）
> 3. 博丽大祭开了后 去神社找时焉侑 目前在于没办法体验第一次博丽大祭 只能通过她来打 有点难受 希望好心人能帮忙）
> 4. 原作者做的金钱修改似乎对新版本不支持 我（wuyulbw）改了一下无济于事 只能通过改存档的方式实现了
> 5. 请以**管理员身份**运行修改器

---

## 用法

选存档 → 选战斗 → 点方案：

| 方案 | 写入位置 | 效果 |
|---|---|---|
| 方案A 排队事件 | `scheduledEvents[当天]`（键 = `playerPartial.gameDate.day`）| 把事件挂进当天的事件桶。读档回到白天场景时游戏会执行当天桶里的待办事件，直接进 Boss 战；当天没反应就结束营业、推进一天再看 |
| 方案B 标记通关 | `finishedEvents` / `finishedMissions` | 把该战标记成已完成，解锁游戏里的「再战」入口，需要自己去对应 NPC / 地点点一下 |
| 方案C 添加邀请函 | 物品 2014~2019 | 刷好感，走原版路线（只有 DLC2 有）|

| 战斗（下拉显示名）| 方案A 事件 | 方案B 判定 | 再战入口 |
|---|---|---|---|
| 与幽幽子的决战 | `Challenge_Finale_P1` | 无判定，恒可用 | 幽幽子羁绊特殊对话 |
| 饕餮挑战赛 | `DLC1_Main_Toutetsu_First_RepeatChallenge_P1` | finishedEvents：`DLC1_Main_Toutetsu_004_Challange_Success` | 妖怪山·荷取对话 |
| 怪诞料理挑战赛 | `DLC2_Main_FormerHell_WeirdCooking_Challenge_P1` | **finishedMissions**：`DLC2_Main_FormerHell_WeirdCooking_Mission_Enter` | 旧地狱·阿燐对话 |
| 博丽大祭 | `DLC3_Repeat_GobackHakureiShrine_Event` | 无判定，恒可用 | 博丽神社·时焉侑 |
| 芙兰朵露挑战赛 | `DLC4_Main_Part10_RepeatChallenge_Begin_Event` | finishedEvents：`DLC4_Main_FlandreCabin_Enter_Event` | 芙兰的房间 |
| 瑞灵 | `DLC5_RepeatChallenge_ArrestMizuchi_Enter_Event` | finishedEvents：`DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event` | 月都控制台 |

原来的「添加邀请函」「触发博丽大祭」两个独立按钮已合并掉：邀请函变成 DLC2 的方案C，博丽大祭直接由方案A 触发。

> 注意：`scheduledEvents` 的键是**日期**（自然存档里能看到 `"62": ["Main_5_BambooForest_001_Event"]` 这种，键 = 该事件要触发的天）。`-1` 是「无日期」桶，游戏拿它放羁绊升级这类事件，往那儿写当天不会执行。方案A 已改为写「当天」的键。

---

## 核心数据

```text
DLC2 角色（ID: 2000~2005）
albumPartialDLC.DLC2.specialSkinSelection

DLC3 角色（ID: 3000~3005）
albumPartialDLC.DLC3.specialSkinSelection

邀请函 ID：2014~2019

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

playerPartial.fund
存档金钱
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

RunTimeScheduler.ScheduleEventExtern(string) -> scheduledEvents
RunTimeScheduler.HaveEventFinished(string)   -> finishedEvents
RunTimeScheduler.HaveMissionFinished(string) -> finishedMissions
```
