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
	ret = SaveEditor_AddInvitations(path);
	if (ret == 0) SaveEditor_SetDLC2Bonds(path);
	return ret;
}

int SaveEditor_SetDLC2Bonds(const char* path)
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

    char* dlc2 = strstr(buf, "\"DLC2\"");
    if (!dlc2) { free(buf); return 0; }
    char* sss = strstr(dlc2, "\"specialSkinSelection\"");
    if (!sss) { free(buf); return -3; }
    char* brace = strchr(sss, '{');
    if (!brace) { free(buf); return -3; }
    int d = 1;
    char* end = brace + 1;
    while (*end && d > 0) {
        if (*end == '{') d++;
        else if (*end == '}') d--;
        end++;
    }
    long region_len = end - brace;

    int ids[] = {2000,2001,2002,2003,2004,2005};
    for (int i = 0; i < 6; i++) {
        char id_str[32];
        _snprintf(id_str, sizeof(id_str), "\"%d\": {", ids[i]);
        char* entry = strstr(brace, id_str);
        if (!entry || entry >= end) continue;
        
        char* ob = strchr(entry, '{');
        if (!ob) continue;
        d = 1;
        char* ee = ob + 1;
        while (*ee && d > 0) {
            if (*ee == '{') d++;
            else if (*ee == '}') d--;
            ee++;
        }
        long entry_len = ee - ob;

        char* exp = strstr(entry, "\"CurrentBondExp\": ");
        if (exp && exp < ee) {
            char* v = exp + 18;
            char* ve = v;
            if (*ve == '-') ve++;
            while (*ve >= '0' && *ve <= '9') ve++;
            long oldl = ve - v;
            if (oldl != 4) {
                memmove(ve + (4-oldl), ve, strlen(ve)+1);
            }
            memcpy(v, "9999", 4);
            long shift = 4 - oldl;
            ee += shift;
            end += shift;
            region_len += shift;
        }

        char* lvl = strstr(entry, "\"CurrentBondLevel\": ");
        if (lvl && lvl < ee) {
            char* v = lvl + 20;
            char* ve = v;
            while (*ve >= '0' && *ve <= '9') ve++;
            long oldl = ve - v;
            if (oldl != 1) {
                memmove(ve + (1-oldl), ve, strlen(ve)+1);
            }
            memcpy(v, "5", 1);
        }
    }

    f = fopen(path, "wb");
    if (!f) { free(buf); return -4; }
    long new_len = strlen(buf);
    fwrite(buf, 1, new_len, f);
    fclose(f);
    free(buf);
    return 0;
}

int SaveEditor_SetDLC3Bonds(const char* path)
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

    char* dlc3 = strstr(buf, "\"DLC3\"");
    if (!dlc3) { free(buf); return 0; }
    char* sss = strstr(dlc3, "\"specialSkinSelection\"");
    if (!sss) { free(buf); return -3; }
    char* brace = strchr(sss, '{');
    if (!brace) { free(buf); return -3; }
    int d = 1;
    char* end = brace + 1;
    while (*end && d > 0) {
        if (*end == '{') d++;
        else if (*end == '}') d--;
        end++;
    }

    int ids[] = {3000,3001,3002,3003,3004,3005};
    for (int i = 0; i < 6; i++) {
        char id_str[32];
        _snprintf(id_str, sizeof(id_str), "\"%d\": {", ids[i]);
        char* entry = strstr(brace, id_str);
        if (!entry || entry >= end) continue;
        
        char* ob = strchr(entry, '{');
        if (!ob) continue;
        d = 1;
        char* ee = ob + 1;
        while (*ee && d > 0) {
            if (*ee == '{') d++;
            else if (*ee == '}') d--;
            ee++;
        }

        char* exp = strstr(entry, "\"CurrentBondExp\": ");
        if (exp && exp < ee) {
            char* v = exp + 18;
            char* ve = v;
            if (*ve == '-') ve++;
            while (*ve >= '0' && *ve <= '9') ve++;
            long oldl = ve - v;
            if (oldl != 4) {
                memmove(ve + (4-oldl), ve, strlen(ve)+1);
            }
            memcpy(v, "9999", 4);
            long shift = 4 - oldl;
            ee += shift;
            end += shift;
        }

        char* lvl = strstr(entry, "\"CurrentBondLevel\": ");
        if (lvl && lvl < ee) {
            char* v = lvl + 20;
            char* ve = v;
            while (*ve >= '0' && *ve <= '9') ve++;
            long oldl = ve - v;
            if (oldl != 1) {
                memmove(ve + (1-oldl), ve, strlen(ve)+1);
            }
            memcpy(v, "5", 1);
        }
    }

    f = fopen(path, "wb");
    if (!f) { free(buf); return -4; }
    long new_len = strlen(buf);
    fwrite(buf, 1, new_len, f);
    fclose(f);
    free(buf);
    return 0;
}

static void InsertAt(char* buf, long* len, long pos, const char* text)
{
    long text_len = strlen(text);
    memmove(buf + pos + text_len, buf + pos, *len - pos + 1);
    memcpy(buf + pos, text, text_len);
    *len += text_len;
}

int SaveEditor_SetKizunaMission(const char* path)
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

    char* sched = strstr(buf, "\"schedulerPartialDLC\"");
    if (!sched) { free(buf); return -3; }
    char* sched_brace = strchr(sched, '{');
    if (!sched_brace) { free(buf); return -3; }
    int d = 1;
    char* sched_end = sched_brace + 1;
    while (*sched_end && d > 0) {
        if (*sched_end == '{') d++;
        else if (*sched_end == '}') d--;
        sched_end++;
    }

    char* dlc3 = strstr(sched_brace, "\"DLC3\"");
    if (!dlc3 || dlc3 >= sched_end) {
        long pos = (sched_end - 1) - buf;
        const char* sec = ",\n  \"DLC3\": {\n    \"dlcSaveDate\": 0,\n    \"scheduledEvents\": {},\n    \"scheduledNews\": {},\n    \"scheduledNewsReplaceContents\": {},\n    \"allTrackingMissions\": {\n      \"0\": {\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }\n    },\n    \"finishedEvents\": [],\n    \"finishedMissions\": [\n      \"DLC3_Main_Part4_KizunaProgress_Mission\"\n    ]\n  }";
        InsertAt(buf, &len, pos, sec);
    } else {
        char* dlc3_brace = strchr(dlc3, '{');
        char* dlc3_end = NULL;
        if (dlc3_brace) {
            dlc3_end = dlc3_brace + 1;
            d = 1;
            while (*dlc3_end && d > 0) {
                if (*dlc3_end == '{') d++;
                else if (*dlc3_end == '}') d--;
                dlc3_end++;
            }
        }
        char* atm = strstr(dlc3, "\"allTrackingMissions\"");
        if (!atm || (dlc3_end && atm >= dlc3_end)) {
            long pos = (dlc3_end - 1) - buf;
            const char* sec = ",\n    \"allTrackingMissions\": {\n      \"0\": {\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }\n    }";
            InsertAt(buf, &len, pos, sec);
        } else {
            char* mission = strstr(atm, "\"DLC3_Main_Part4_KizunaProgress_Mission\"");
            char* atm_brace = strchr(atm, '{');
            char* atm_end = NULL;
            if (atm_brace) {
                atm_end = atm_brace + 1;
                d = 1;
                while (*atm_end && d > 0) {
                    if (*atm_end == '{') d++;
                    else if (*atm_end == '}') d--;
                    atm_end++;
                }
            }
            if (!mission || (atm_end && mission >= atm_end)) {
                long pos = (atm_end - 1) - buf;
                const char* comma = (atm_end && *(atm_end - 1) == '{') ? "" : ",";
                char entry[512];
                _snprintf(entry, sizeof(entry), "%s\n      \"0\": {\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }", comma);
                InsertAt(buf, &len, pos, entry);
            } else {
                char* cfs = strstr(mission, "\"conditionFinishStates\"");
                if (cfs) {
                    char* arr = strchr(cfs, '[');
                    if (arr) {
                        d = 1;
                        char* arr_end = arr + 1;
                        while (*arr_end && d > 0) {
                            if (*arr_end == '[') d++;
                            else if (*arr_end == ']') d--;
                            arr_end++;
                        }
                        const char* na = "[true, true, true, true, true, true]";
                        memmove(arr + strlen(na), arr_end, strlen(arr_end)+1);
                        memcpy(arr, na, strlen(na));
                        len = strlen(buf);
                    }
                }
            }
        }
        // Add to finishedMissions if not there
        char* fin = strstr(dlc3, "\"finishedMissions\"");
        if (fin && fin < (dlc3_end ? dlc3_end : fin + 100)) {
            char* check = strstr(fin, "\"DLC3_Main_Part4_KizunaProgress_Mission\"");
            if (!check) {
                char* fb = strchr(fin, '[');
                if (fb) {
                    d = 1;
                    char* fe = fb + 1;
                    while (*fe && d > 0) {
                        if (*fe == '[') d++;
                        else if (*fe == ']') d--;
                        fe++;
                    }
                    long pos = (fe - 1) - buf;
                    const char* comma = (fe && *(fe - 1) == '[') ? "" : ",";
                    char entry[128];
                    _snprintf(entry, sizeof(entry), "%s\n      \"DLC3_Main_Part4_KizunaProgress_Mission\"", comma);
                    InsertAt(buf, &len, pos, entry);
                }
            }
        }
    }

    f = fopen(path, "wb");
    if (!f) { free(buf); return -4; }
    fwrite(buf, 1, len, f);
    fclose(f);
    free(buf);
    return 0;
}

int SaveEditor_TriggerFestival(const char* path)
{
    int ret = SaveEditor_SetDLC3Bonds(path);
    ret = SaveEditor_SetKizunaMission(path);
    return (ret == 0 || ret == -3) ? 0 : ret;
}

int SaveEditor_TriggerFestivalSlot(int slot)
{
    char path[MAX_PATH];
    int ret = SaveEditor_GetPath(slot, path, sizeof(path));
    if (ret) return ret;
    return SaveEditor_TriggerFestival(path);
}