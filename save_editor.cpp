#include "save_editor.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static const int INV_IDS[] = {2014, 2015, 2016, 2017, 2018, 2019};
static const int INV_COUNT = 6;
static const char* SAVE_SUBDIR = "BetaV9";
// DLC2/DLC3 bond entries: id ranges and the exp value written per DLC.
static const int DLC2_BOND_IDS[] = {2000, 2001, 2002, 2003, 2004, 2005};
static const int DLC3_BOND_IDS[] = {3000, 3001, 3002, 3003, 3004, 3005};
static const int BOND_ID_COUNT = 6;
static const char* DLC2_BOND_EXP_VALUE = "9999";
static const char* DLC3_BOND_EXP_VALUE = "400";
static const char* BOND_LVL_VALUE = "5";
static const char BOND_EXP_KEY[] = "\"CurrentBondExp\": ";
static const char BOND_LVL_KEY[] = "\"CurrentBondLevel\": ";
// Extra bytes past the file size when loading a save for in-place edits;
// must cover every insertion performed on the buffer.
static const long FILE_SLACK = 16384;

// Returns the pointer just past the closing brace of the JSON object starting
// at brace (or the NUL terminator when the object is unbalanced).
static char* FindObjectEnd(char* brace)
{
    int depth = 1;
    char* p = brace + 1;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
    }
    return p;
}

// Same as FindObjectEnd for a JSON array starting at bracket.
static char* FindArrayEnd(char* bracket)
{
    int depth = 1;
    char* p = bracket + 1;
    while (*p && depth > 0) {
        if (*p == '[') depth++;
        else if (*p == ']') depth--;
        p++;
    }
    return p;
}

// Loads the whole file into a NUL-terminated buffer with FILE_SLACK bytes of
// room for in-place edits. On failure returns NULL and sets *outLen to the
// error code (-1 open, -2 alloc or short read).
static char* LoadFileForEdit(const char* path, long* outLen)
{
    *outLen = -1;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + FILE_SLACK);
    if (!buf) { fclose(f); *outLen = -2; return NULL; }
    size_t bytesRead = fread(buf, 1, len, f);
    fclose(f);
    if ((long)bytesRead != len) { free(buf); *outLen = -2; return NULL; }
    buf[len] = '\0';
    *outLen = len;
    return buf;
}

static int SaveEditedFile(const char* path, const char* buf, long len)
{
    FILE* f = fopen(path, "wb");
    if (!f) return -4;
    fwrite(buf, 1, len, f);
    fclose(f);
    return 0;
}

// Overwrites the (optionally negative) decimal number starting at v with
// digits, shifting the rest of the buffer as needed. Returns the byte shift
// applied to the text following the number.
static long OverwriteNumber(char* v, const char* digits)
{
    char* ve = v;
    if (*ve == '-') ve++;
    while (*ve >= '0' && *ve <= '9') ve++;
    long oldLen = ve - v;
    long newLen = (long)strlen(digits);
    if (newLen != oldLen) {
        memmove(ve + (newLen - oldLen), ve, strlen(ve) + 1);
    }
    memcpy(v, digits, newLen);
    return newLen - oldLen;
}

// Replaces CurrentBondExp/CurrentBondLevel of the bond entry spanning
// [entry, *entryEnd). *entryEnd is kept valid across the shifts. Returns the
// total byte shift applied to the text after the entry.
static long SetBondEntryNumbers(char* entry, char** entryEnd, const char* expValue)
{
    long shift = 0;
    char* exp = strstr(entry, BOND_EXP_KEY);
    if (exp && exp < *entryEnd) {
        shift += OverwriteNumber(exp + strlen(BOND_EXP_KEY), expValue);
        *entryEnd += shift;
    }
    char* lvl = strstr(entry, BOND_LVL_KEY);
    if (lvl && lvl < *entryEnd) {
        shift += OverwriteNumber(lvl + strlen(BOND_LVL_KEY), BOND_LVL_VALUE);
        *entryEnd += shift;
    }
    return shift;
}

// Sets the bond exp/level of every id in ids within the JSON object spanning
// [brace, *end). *end is adjusted in place because replacements shift the
// buffer contents.
static void SetBondEntries(char* brace, char** end, const int* ids, int count, const char* expValue)
{
    for (int i = 0; i < count; i++) {
        char id_str[32];
        _snprintf(id_str, sizeof(id_str), "\"%d\": {", ids[i]);
        char* entry = strstr(brace, id_str);
        if (!entry || entry >= *end) continue;
        char* ob = strchr(entry, '{');
        if (!ob) continue;
        char* entryEnd = FindObjectEnd(ob);
        *end += SetBondEntryNumbers(entry, &entryEnd, expValue);
    }
}

// Inserts text at pos, growing *len; refuses when it would exceed cap bytes.
// Returns true on success.
static bool InsertAt(char* buf, long cap, long* len, long pos, const char* text)
{
    long text_len = (long)strlen(text);
    if (*len + text_len + 1 > cap) return false;
    memmove(buf + pos + text_len, buf + pos, *len - pos + 1);
    memcpy(buf + pos, text, text_len);
    *len += text_len;
    return true;
}

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
    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;
    long cap = len + FILE_SLACK;

    char* storage = strstr(buf, "\"storagePartial\"");
    if (!storage) { free(buf); return -3; }
    char* items_start = strstr(storage, "\"items\"");
    if (!items_start) { free(buf); return -3; }
    char* brace = strchr(items_start, '{');
    if (!brace) { free(buf); return -3; }
    char* items_close = FindObjectEnd(brace) - 1;

    long insert_pos = items_close - buf;
    for (int i = 0; i < INV_COUNT; i++) {
        // The comma decision must reflect the buffer as it is now; a stale
        // pointer produced invalid JSON when the items object started empty.
        int has_comma = (insert_pos > 0 && buf[insert_pos - 1] != '{');
        char entry[32];
        _snprintf(entry, sizeof(entry), "%s\n    \"%d\": 1", has_comma ? "," : "", INV_IDS[i]);
        if (!InsertAt(buf, cap, &len, insert_pos, entry)) break;
        insert_pos += (long)strlen(entry);
    }

    int ret = SaveEditedFile(path, buf, len);
    free(buf);
    return ret;
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

// Shares the DLC3 bond-writing logic with SaveEditor_TriggerFestival; when
// fromAlbum is true the DLC3 object is looked up under "albumPartialDLC",
// otherwise directly in the document root.
static int SetDLC2BondsImpl(const char* path)
{
    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;

    char* dlc2 = strstr(buf, "\"DLC2\"");
    if (!dlc2) { free(buf); return 0; }
    char* sss = strstr(dlc2, "\"specialSkinSelection\"");
    if (!sss) { free(buf); return -3; }
    char* brace = strchr(sss, '{');
    if (!brace) { free(buf); return -3; }
    char* end = FindObjectEnd(brace);

    SetBondEntries(brace, &end, DLC2_BOND_IDS, BOND_ID_COUNT, DLC2_BOND_EXP_VALUE);

    int ret = SaveEditedFile(path, buf, (long)strlen(buf));
    free(buf);
    return ret;
}

int SaveEditor_SetDLC2Bonds(const char* path)
{
    return SetDLC2BondsImpl(path);
}

static int SetDLC3BondsImpl(const char* path)
{
    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;

    char* album = strstr(buf, "\"albumPartialDLC\"");
    if (!album) { free(buf); return 0; }
    char* dlc3 = strstr(album, "\"DLC3\"");
    if (!dlc3) { free(buf); return 0; }
    char* sss = strstr(dlc3, "\"specialSkinSelection\"");
    if (!sss) { free(buf); return -3; }
    char* brace = strchr(sss, '{');
    if (!brace) { free(buf); return -3; }
    char* end = FindObjectEnd(brace);

    SetBondEntries(brace, &end, DLC3_BOND_IDS, BOND_ID_COUNT, DLC3_BOND_EXP_VALUE);

    int ret = SaveEditedFile(path, buf, (long)strlen(buf));
    free(buf);
    return ret;
}

int SaveEditor_SetDLC3Bonds(const char* path)
{
    return SetDLC3BondsImpl(path);
}

int SaveEditor_TriggerFestival(const char* path)
{
    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;
    long cap = len + FILE_SLACK;

    // 1. Set DLC3 bonds
    char* album = strstr(buf, "\"albumPartialDLC\"");
    if (album) {
        char* dlc3 = strstr(album, "\"DLC3\"");
        if (dlc3) {
            char* sss = strstr(dlc3, "\"specialSkinSelection\"");
            if (sss) {
                char* brace = strchr(sss, '{');
                if (brace) {
                    char* end = FindObjectEnd(brace);
                    SetBondEntries(brace, &end, DLC3_BOND_IDS, BOND_ID_COUNT, DLC3_BOND_EXP_VALUE);
                }
            }
        }
    }

    // 2. Replace trackedSwitch
    char* ts = strstr(buf, "\"trackedSwitch\"");
    if (ts) {
        char* brace = strchr(ts, '{');
        if (brace) {
            char* end = FindObjectEnd(brace);
            const char* new_ts =
                "\"trackedSwitch\": {\n"
                "      \"Aya_FamousIzakaya\": false,\n"
                "      \"Lantern_A_Display\": true,\n"
                "      \"Lantern_B_Display\": true,\n"
                "      \"Lantern_C_Display\": true,\n"
                "      \"Lantern_D_Display\": true,\n"
                "      \"Lantern_E_Display\": true,\n"
                "      \"MengChengGuo\": true,\n"
                "      \"3Faries\": true,\n"
                "      \"DLC2.5_MusicMachine\": true,\n"
                "      \"DLC3_Main_Part3_PalmCivet\": false,\n"
                "      \"Kyouko_Tutorial_Top\": true,\n"
                "      \"Kyouko_Tutorial_Preset\": true,\n"
                "      \"Kyouko_Tutorial_Showcase\": true,\n"
                "      \"Kyouko_Tutorial_Closet\": true,\n"
                "      \"Kyouko_Tutorial_CDPlayer\": true,\n"
                "      \"Kyouko_Tutorial_SpellCard\": true,\n"
                "      \"Kyouko_Tutorial_Kourindou\": true,\n"
                "      \"Kyouko_Tutorial_Hakugyokurou\": false,\n"
                "      \"Kyouko_Tutorial_DLC\": true,\n"
                "      \"Kyouko_Tutorial_ForDLC1MainStory\": false,\n"
                "      \"HumanVillage_Farmland_A_Disabled\": false,\n"
                "      \"HumanVillage_Farmland_B_Disabled\": false,\n"
                "      \"HumanVillage_Farmland_C_Disabled\": false,\n"
                "      \"DLC1_MagicForest_MagicTree_Green\": false,\n"
                "      \"DLC5_Map_Makai_Portal\": false,\n"
                "      \"Aunn_Stone\": true,\n"
                "      \"DLC5_Main_Part6_Tenshi\": false,\n"
                "      \"DLC3_HakureiFestival_RepeatChallenge_JienYuuCharacter\": true,\n"
                "      \"DLC3_HakureiFestival_JienYuu\": true,\n"
                "      \"Daiyousei_Ice\": false,\n"
                "      \"Sakuya_Door\": true,\n"
                "      \"TBC2_Collab_Has_Interact\": true,\n"
                "      \"3FARIES_Collab_Has_Interact\": true,\n"
                "      \"MC_Gensokyo_Has_Interact\": true,\n"
                "      \"TBS_Kokoro\": false,\n"
                "      \"TBS_Kokoro_Has_Interact\": true,\n"
                "      \"TRACKED_SWITCH_RINNOSUKE_WELCOME\": false,\n"
                "      \"TRACKED_SWITCH_RINNOSUKE_GETCOUPLE\": true,\n"
                "      \"THYG_Has_Interact\": true\n"
                "    }";
            long old_len = end - ts;
            long new_len = (long)strlen(new_ts);
            long shift = new_len - old_len;
            if (len + shift + 1 > cap) { free(buf); return -2; }
            memmove(end + shift, end, strlen(end) + 1);
            memcpy(ts, new_ts, new_len);
            len += shift;
        }
    }

    // 3. Set mission and add events
    char* sched = strstr(buf, "\"schedulerPartialDLC\"");
    if (sched) {
        char* sched_brace = strchr(sched, '{');
        if (sched_brace) {
            char* sched_end = FindObjectEnd(sched_brace);
            char* dlc3 = strstr(sched_brace, "\"DLC3\"");
            if (!dlc3 || dlc3 >= sched_end) {
                long pos = (sched_end - 1) - buf;
                const char* comma = (sched_brace && *(sched_brace + 1) == '}') ? "" : ",";
                char section[8192];
                _snprintf(section, sizeof(section),
                    "%s\n  \"DLC3\": {\n    \"dlcSaveDate\": 0,\n    \"scheduledEvents\": {},\n    \"scheduledNews\": {},\n    \"scheduledNewsReplaceContents\": {},\n    \"allTrackingMissions\": {\n      \"0\": [{\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }]\n    },\n    \"finishedEvents\": [\n      \"DLC3_Main_Part4_Mission_Finished_Event\"\n    ],\n    \"finishedMissions\": [\n      \"DLC3_Main_Part4_KizunaProgress_Mission\",\n      \"DLC3_Main_Part8_HakureiFestivalChallenge_GuidedMission\"\n    ]\n  }", comma);
                InsertAt(buf, cap, &len, pos, section);
            } else {
                char* dlc3_brace = strchr(dlc3, '{');
                if (dlc3_brace) {
                    char* dlc3_end = FindObjectEnd(dlc3_brace);
                    // Find or create allTrackingMissions
                    char* atm = strstr(dlc3, "\"allTrackingMissions\"");
                    if (!atm || atm >= dlc3_end) {
                        long pos = (dlc3_end - 1) - buf;
                        const char* sec = ",\n    \"allTrackingMissions\": {\n      \"0\": [{\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }]\n    }";
                        InsertAt(buf, cap, &len, pos, sec);
                    } else {
                        // Update conditionFinishStates
                        char* mission = strstr(atm, "\"DLC3_Main_Part4_KizunaProgress_Mission\"");
                        if (mission) {
                            char* cfs = strstr(mission, "\"conditionFinishStates\"");
                            if (cfs) {
                                char* arr = strchr(cfs, '[');
                                if (arr) {
                                    char* arr_end = FindArrayEnd(arr);
                                    const char* na = "[true, true, true, true, true, true]";
                                    long old_len = arr_end - arr;
                                    long new_len = (long)strlen(na);
                                    long shift = new_len - old_len;
                                    if (len + shift + 1 <= cap) {
                                        memmove(arr + new_len, arr_end, strlen(arr_end) + 1);
                                        memcpy(arr, na, new_len);
                                        len += shift;
                                    }
                                }
                            }
                        } else {
                            // Add mission entry to atm
                            char* atm_brace = strchr(atm, '{');
                            char* atm_end = NULL;
                            if (atm_brace) {
                                atm_end = FindObjectEnd(atm_brace);
                            }
                            if (atm_end) {
                                long pos = (atm_end - 1) - buf;
                                const char* comma = (*(atm_brace + 1) == '}') ? "" : ",";
                                char entry[512];
                                _snprintf(entry, sizeof(entry), "%s\n      \"0\": [{\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }]", comma);
                                InsertAt(buf, cap, &len, pos, entry);
                            }
                        }
                    }

                    // Update finishedEvents - find if already has DLC3_Main_Part4_Mission_Finished_Event
                    char* fe = strstr(dlc3, "\"finishedEvents\"");
                    if (fe) {
                        char* event_check = strstr(fe, "\"DLC3_Main_Part4_Mission_Finished_Event\"");
                        if (!event_check) {
                            char* fb = strchr(fe, '[');
                            if (fb) {
                                char* fe_end = FindArrayEnd(fb);
                                long pos = (fe_end - 1) - buf;
                                const char* comma = (fb && *(fb + 1) == ']') ? "" : ",";
                                char entry[128];
                                _snprintf(entry, sizeof(entry), "%s\n      \"DLC3_Main_Part4_Mission_Finished_Event\"", comma);
                                InsertAt(buf, cap, &len, pos, entry);
                            }
                        }
                    }

                    // Update finishedMissions
                    char* fin = strstr(dlc3, "\"finishedMissions\"");
                    if (fin) {
                        const char* needed_missions[] = {
                            "DLC3_Main_Part4_KizunaProgress_Mission",
                            "DLC3_Main_Part4.5.3_GuidedMission",
                            "DLC3_Main_Part8_HakureiFestivalChallenge_GuidedMission"
                        };
                        for (int i = 0; i < 3; i++) {
                            char* check = strstr(fin, needed_missions[i]);
                            if (!check) {
                                char* fb = strchr(fin, '[');
                                if (fb) {
                                    char* fm_end = FindArrayEnd(fb);
                                    long pos = (fm_end - 1) - buf;
                                    const char* comma = (fb && *(fb + 1) == ']') ? "" : ",";
                                    char entry[128];
                                    _snprintf(entry, sizeof(entry), "%s\n      \"%s\"", comma, needed_missions[i]);
                                    InsertAt(buf, cap, &len, pos, entry);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    int ret = SaveEditedFile(path, buf, len);
    free(buf);
    return ret;
}

int SaveEditor_TriggerFestivalSlot(int slot)
{
    char path[MAX_PATH];
    int ret = SaveEditor_GetPath(slot, path, sizeof(path));
    if (ret) return ret;
    return SaveEditor_TriggerFestival(path);
}

int SaveEditor_SetFund(const char* path, int value)
{
    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;

    char* fund = strstr(buf, "\"fund\"");
    if (!fund) { free(buf); return -3; }
    char* colon = strchr(fund, ':');
    if (!colon) { free(buf); return -3; }
    char* val = colon + 1;
    while (*val == ' ') val++;

    char new_val[32];
    _snprintf(new_val, sizeof(new_val), "%d", value);
    OverwriteNumber(val, new_val);

    int ret = SaveEditedFile(path, buf, (long)strlen(buf));
    free(buf);
    return ret;
}
