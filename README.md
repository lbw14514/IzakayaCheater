# IzakayaCheater

东方夜雀食堂修改器，目前可以修改金钱，强开 DLC2 / DLC3 最终 Boss 战。

目前支持版本：鬼知道……（反正支持4.4.0）

---

## 为啥写这东西（对我个人来说）

DLC2 和 DLC3 的 Boss 战需要 12 角色满好感，太累了，所以写了。

---

## 注意事项（必看）

> [!CAUTION]
> **请一定一定先备份存档 然后再操作**  


> [!WARNING]
> 1. 目前只做了DLC2和3的BOSS战解锁
> 
>  DLC1的饕餮你本体过完开了演唱会去转转 等几天就开了 遂不做
> 
>  DLC4和5的BOSS后续会做 现在不做是因为我没有符合条件的存档（抱歉啦）
> 
> 2. 开启「博丽大祭」时 如果你的进度未到狸猫的情报大作战 可能会丢失 DLC3 未完成的进度
> 3. 博丽大祭开了后 去神社找时焉侑 目前在于没办法体验第一次博丽大祭 只能通过她来打 有点难受 希望好心人能帮忙）
> 4. 原作者做的金钱修改似乎对新版本不支持 我（wuyulbw）改了一下无济于事 只能通过改存档的方式实现了
> 5. 请以**管理员身份**运行修改器

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
