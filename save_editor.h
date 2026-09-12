#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int SaveEditor_GetPath(int slot, char* path, DWORD size);
int SaveEditor_AddInvitations(const char* path);
int SaveEditor_AddInvitationsToSlot(int slot);
int SaveEditor_GetSaveFolder(char* folder, DWORD size);
int SaveEditor_ScanSaves(int* slots, int* count, int maxCount);
int SaveEditor_SetFund(const char* path, int value);
int SaveEditor_SetDLC2Bonds(const char* path);
int SaveEditor_SetDLC3Bonds(const char* path);
int SaveEditor_SetKizunaMission(const char* path);
int SaveEditor_TriggerFestival(const char* path);
int SaveEditor_TriggerFestivalSlot(int slot);

int SaveEditor_GetBossCount(void);
const char* SaveEditor_GetBossLabel(int bossId);
const char* SaveEditor_GetBossMethods(int bossId);
const char* SaveEditor_GetBossDesc(int bossId);
int SaveEditor_BossHasQueue(int bossId);
int SaveEditor_BossHasClear(int bossId);
int SaveEditor_BossHasInvite(int bossId);
int SaveEditor_QueueBossEvents(const char* path, int bossId);
int SaveEditor_SetBossCleared(const char* path, int bossId);
int SaveEditor_UnlockAllMaps(const char* path);
int SaveEditor_GetMapCount(void);
int SaveEditor_MaxAllBonds(const char* path);

#ifdef __cplusplus
}
#endif
