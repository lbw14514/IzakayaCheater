#include "save_editor.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static const int INV_IDS[] = {2014, 2015, 2016, 2017, 2018, 2019};
static const int INV_COUNT = 6;
static const char* SAVE_SUBDIR = "BetaV9";

int SaveEditor_GetPath(int slot, char* path, DWORD size)
{
	char folder[MAX_PATH];
	if (SaveEditor_GetSaveFolder(folder, sizeof(folder)))
		return -1;
	_snprintf(path, size, "%s\\Mystia#%d.memory", folder, slot);
	return 0;
}

int SaveEditor_AddInvitations(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return -1;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = (char*)malloc(len + 4096);
	if (!buf) { fclose(f); return -2; }
	fread(buf, 1, len, f);
	fclose(f);
	buf[len] = '\0';

	char* storage = strstr(buf, "\"storagePartial\"");
	if (!storage) { free(buf); return -3; }

	char* items_start = strstr(storage, "\"items\"");
	if (!items_start) { free(buf); return -3; }

	char* brace = strchr(items_start, '{');
	if (!brace) { free(buf); return -3; }

	int depth = 1;
	char* items_end = brace + 1;
	while (*items_end && depth > 0) {
		if (*items_end == '{') depth++;
		else if (*items_end == '}') depth--;
		items_end++;
	}
	items_end--;

	long insert_pos = items_end - buf;

	for (int i = 0; i < INV_COUNT; i++) {
		char id_str[16];
		_snprintf(id_str, sizeof(id_str), "\"%d\"", INV_IDS[i]);
		if (strstr(buf, id_str))
			continue;

		char entry[32];
		int has_comma = (items_end > brace && *(items_end - 1) != '{');
		_snprintf(entry, sizeof(entry), "%s\n    \"%d\": 1", has_comma ? "," : "", INV_IDS[i]);

		memmove(buf + insert_pos + strlen(entry), buf + insert_pos, len - insert_pos + 1);
		memcpy(buf + insert_pos, entry, strlen(entry));
		insert_pos += strlen(entry);
		len += strlen(entry);
	}

	f = fopen(path, "wb");
	if (!f) { free(buf); return -4; }
	fwrite(buf, 1, len, f);
	fclose(f);
	free(buf);
	return 0;
}

int SaveEditor_GetSaveFolder(char* folder, DWORD size)
{
	char userProfile[MAX_PATH];
	if (!GetEnvironmentVariableA("USERPROFILE", userProfile, sizeof(userProfile)))
		return -1;
	_snprintf(folder, size, "%s\\AppData\\LocalLow\\Epicomic\\Touhou Mystia Izakaya\\Memory\\Save\\%s",
	          userProfile, SAVE_SUBDIR);
	return 0;
}

int SaveEditor_ScanSaves(int* slots, int* count, int maxCount)
{
	char folder[MAX_PATH];
	if (SaveEditor_GetSaveFolder(folder, sizeof(folder)))
		return -1;

	char pattern[MAX_PATH];
	_snprintf(pattern, sizeof(pattern), "%s\\Mystia#*.memory", folder);

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(pattern, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return -1;

	*count = 0;
	do {
		int slot = 0;
		if (sscanf(findData.cFileName, "Mystia#%d.memory", &slot) == 1) {
			if (*count < maxCount)
				slots[(*count)++] = slot;
		}
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);
	return 0;
}

int SaveEditor_AddInvitationsToSlot(int slot)
{
	char path[MAX_PATH];
	int ret = SaveEditor_GetPath(slot, path, sizeof(path));
	if (ret) return ret;
	return SaveEditor_AddInvitations(path);
}
