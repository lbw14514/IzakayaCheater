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

#ifdef __cplusplus
}
#endif
