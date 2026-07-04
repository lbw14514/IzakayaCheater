#include "save_editor.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static const int INV_IDS[] = {2014, 2015, 2016, 2017, 2018, 2019};
static const int INV_COUNT = 6;
static const char* SAVE_SUBDIR = "BetaV9";

static void InsertAt(char* buf, long* len, long pos, const char* text)
{
    long text_len = strlen(text);
    memmove(buf + pos + text_len, buf + pos, *len - pos + 1);
    memcpy(buf + pos, text, text_len);
    *len += text_len;
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

    char* album = strstr(buf, "\"albumPartialDLC\"");
    if (!album) { free(buf); return 0; }
    char* dlc3 = strstr(album, "\"DLC3\"");
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
            if (oldl != 3) {
                memmove(ve + (3-oldl), ve, strlen(ve)+1);
            }
            memcpy(v, "400", 3);
            long shift = 3 - oldl;
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

int SaveEditor_TriggerFestival(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(len + 16384);
    if (!buf) { fclose(f); return -2; }
    fread(buf, 1, len, f);
    fclose(f);
    buf[len] = '\0';

    // 1. Set DLC3 bonds
    char* album = strstr(buf, "\"albumPartialDLC\"");
    if (album) {
        char* dlc3 = strstr(album, "\"DLC3\"");
        if (dlc3) {
            char* sss = strstr(dlc3, "\"specialSkinSelection\"");
            if (sss) {
                char* brace = strchr(sss, '{');
                if (brace) {
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
                            if (oldl != 3) {
                                memmove(ve + (3-oldl), ve, strlen(ve)+1);
                            }
                            memcpy(v, "400", 3);
                            long shift = 3 - oldl;
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
                }
            }
        }
    }

    // 2. Replace trackedSwitch
    char* ts = strstr(buf, "\"trackedSwitch\"");
    if (ts) {
        char* brace = strchr(ts, '{');
        if (brace) {
            int d = 1;
            char* end = brace + 1;
            while (*end && d > 0) {
                if (*end == '{') d++;
                else if (*end == '}') d--;
                end++;
            }
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
            long new_len = strlen(new_ts);
            long shift = new_len - old_len;
            memmove(end + shift, end, strlen(end) + 1);
            memcpy(ts, new_ts, new_len);
            len = strlen(buf);
        }
    }

    // 3. Set mission and add events
    char* sched = strstr(buf, "\"schedulerPartialDLC\"");
    if (sched) {
        char* sched_brace = strchr(sched, '{');
        if (sched_brace) {
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
                const char* comma = (sched_brace && *(sched_brace + 1) == '}') ? "" : ",";
                char section[8192];
                _snprintf(section, sizeof(section),
                    "%s\n  \"DLC3\": {\n    \"dlcSaveDate\": 0,\n    \"scheduledEvents\": {},\n    \"scheduledNews\": {},\n    \"scheduledNewsReplaceContents\": {},\n    \"allTrackingMissions\": {\n      \"0\": [{\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }]\n    },\n    \"finishedEvents\": [\n      \"DLC3_Main_Part4_Mission_Finished_Event\"\n    ],\n    \"finishedMissions\": [\n      \"DLC3_Main_Part4_KizunaProgress_Mission\",\n      \"DLC3_Main_Part8_HakureiFestivalChallenge_GuidedMission\"\n    ]\n  }", comma);
                InsertAt(buf, &len, pos, section);
            } else {
                char* dlc3_brace = strchr(dlc3, '{');
                if (dlc3_brace) {
                    d = 1;
                    char* dlc3_end = dlc3_brace + 1;
                    while (*dlc3_end && d > 0) {
                        if (*dlc3_end == '{') d++;
                        else if (*dlc3_end == '}') d--;
                        dlc3_end++;
                    }
                    // Find or create allTrackingMissions
                    char* atm = strstr(dlc3, "\"allTrackingMissions\"");
                    if (!atm || atm >= dlc3_end) {
                        long pos = (dlc3_end - 1) - buf;
                        const char* sec = ",\n    \"allTrackingMissions\": {\n      \"0\": [{\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }]\n    }";
                        InsertAt(buf, &len, pos, sec);
                        len = strlen(buf);
                        dlc3_end = NULL;
                    } else {
                        // Update conditionFinishStates
                        char* mission = strstr(atm, "\"DLC3_Main_Part4_KizunaProgress_Mission\"");
                        if (mission) {
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
                        } else {
                            // Add mission entry to atm
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
                            long pos = (atm_end - 1) - buf;
                            const char* comma = (atm_brace && *(atm_brace + 1) == '}') ? "" : ",";
                            char entry[512];
                            _snprintf(entry, sizeof(entry), "%s\n      \"0\": [{\n        \"missionLabel\": \"DLC3_Main_Part4_KizunaProgress_Mission\",\n        \"conditionFinishStates\": [true, true, true, true, true, true],\n        \"conditionData\": [[],[],[],[],[],[]]\n      }]", comma);
                            InsertAt(buf, &len, pos, entry);
                            len = strlen(buf);
                        }
                    }

                    // Update finishedEvents - find if already has DLC3_Main_Part4_Mission_Finished_Event
                    char* fe = strstr(dlc3, "\"finishedEvents\"");
                    if (fe) {
                        char* event_check = strstr(fe, "\"DLC3_Main_Part4_Mission_Finished_Event\"");
                        if (!event_check) {
                            char* fb = strchr(fe, '[');
                            if (fb) {
                                d = 1;
                                char* fe_end = fb + 1;
                                while (*fe_end && d > 0) {
                                    if (*fe_end == '[') d++;
                                    else if (*fe_end == ']') d--;
                                    fe_end++;
                                }
                                long pos = (fe_end - 1) - buf;
                                const char* comma = (fb && *(fb + 1) == ']') ? "" : ",";
                                char entry[128];
                                _snprintf(entry, sizeof(entry), "%s\n      \"DLC3_Main_Part4_Mission_Finished_Event\"", comma);
                                InsertAt(buf, &len, pos, entry);
                                len = strlen(buf);
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
                                    d = 1;
                                    char* fm_end = fb + 1;
                                    while (*fm_end && d > 0) {
                                        if (*fm_end == '[') d++;
                                        else if (*fm_end == ']') d--;
                                        fm_end++;
                                    }
                                    long pos = (fm_end - 1) - buf;
                                    const char* comma = (fb && *(fb + 1) == ']') ? "" : ",";
                                    char entry[128];
                                    _snprintf(entry, sizeof(entry), "%s\n      \"%s\"", comma, needed_missions[i]);
                                    InsertAt(buf, &len, pos, entry);
                                    len = strlen(buf);
                                }
                            }
                        }
                    }
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

int SaveEditor_TriggerFestivalSlot(int slot)
{
    char path[MAX_PATH];
    int ret = SaveEditor_GetPath(slot, path, sizeof(path));
    if (ret) return ret;
    return SaveEditor_TriggerFestival(path);
}

int SaveEditor_SetFund(const char* path, int value)
{
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 128);
    if (!buf) { fclose(f); return -2; }
    fread(buf, 1, len, f);
    fclose(f);
    buf[len] = '\0';

    char* fund = strstr(buf, "\"fund\"");
    if (!fund) { free(buf); return -3; }
    char* colon = strchr(fund, ':');
    if (!colon) { free(buf); return -3; }
    char* val = colon + 1;
    while (*val == ' ') val++;
    char* val_end = val;
    if (*val_end == '-') val_end++;
    while (*val_end >= '0' && *val_end <= '9') val_end++;

    char new_val[32];
    _snprintf(new_val, sizeof(new_val), "%d", value);
    long old_len = val_end - val;
    long new_len = strlen(new_val);
    if (new_len != old_len) {
        memmove(val_end + (new_len - old_len), val_end, strlen(val_end) + 1);
    }
    memcpy(val, new_val, new_len);
    len = strlen(buf);

    f = fopen(path, "wb");
    if (!f) { free(buf); return -4; }
    fwrite(buf, 1, len, f);
    fclose(f);
    free(buf);
    return 0;
}
