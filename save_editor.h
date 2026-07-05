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

#ifdef __cplusplus
}
#endif
