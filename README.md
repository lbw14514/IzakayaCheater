# IzakayaCheater
东方夜雀食堂修改器，目前可以修改金钱，强开DLC2/DLC3最终boss战

目前支持版本：鬼知道……

## 为啥写这东西（对我个人来说）
dlc2和3的boss战需要12角色满好感 太累了 所以写了

## 注意
> ** 1.一定一定要备份**
>2.开博丽大祭时 如果你的进度没到狸猫的情报大作战这块 可能丢失dlc3没有过的进度
>3.博丽大祭开了后 去神社找时焉侑 目前在于没办法体验第一次博丽大祭 只能通过她来打 有点难受 希望好心人能帮忙）
>4.点击获取邀请函后 似乎需要重读两次档才能有效果 不知道啥原理
>5.管理员运行

## 核心数据
DLC2 角色（ID 2000~2005）  albumPartialDLC.DLC2.specialSkinSelection
DLC3 角色（ID 3000~3005）  albumPartialDLC.DLC3.specialSkinSelection
邀请函 ID：2014~2019
39 项开关，控制游戏内各种状态  dayScenePartial.trackedSwitch
DLC3_HakureiFestival_JienYuu: true  博丽大祭所需字段
DLC3_HakureiFestival_RepeatChallenge_JienYuuCharacter: true  博丽大祭所需字段
schedulerPartialDLC.DLC3.allTrackingMissions["0"]  任务跟踪列表，值为数组 [{...}]
schedulerPartialDLC.DLC3.finishedMissions  已完成任务列表
schedulerPartialDLC.DLC3.finishedEvents  	已完成事件列表
