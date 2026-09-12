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
// buffer contents. Returns the total shift applied to the whole buffer.
static long SetBondEntries(char* brace, char** end, const int* ids, int count, const char* expValue)
{
    long shift = 0;
    for (int i = 0; i < count; i++) {
        char id_str[32];
        _snprintf(id_str, sizeof(id_str), "\"%d\": {", ids[i]);
        char* entry = strstr(brace, id_str);
        if (!entry || entry >= *end) continue;
        char* ob = strchr(entry, '{');
        if (!ob) continue;
        char* entryEnd = FindObjectEnd(ob);
        long applied = SetBondEntryNumbers(entry, &entryEnd, expValue);
        *end += applied;
        shift += applied;
    }
    return shift;
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

static void ReplaceRange(char* buf, long cap, long* len, long pos, long oldLen, const char* text)
{
    long newLen = (long)strlen(text);
    long shift = newLen - oldLen;
    if (*len + shift + 1 > cap) return;
    memmove(buf + pos + newLen, buf + pos + oldLen, *len - (pos + oldLen) + 1);
    memcpy(buf + pos, text, newLen);
    *len += shift;
}

static char* SkipBackWs(char* p, char* stop)
{
    while (p > stop && (p[-1] == ' ' || p[-1] == '\n' || p[-1] == '\r' || p[-1] == '\t')) p--;
    return p;
}

static bool IsEmptyObject(char* openBrace)
{
    return SkipBackWs(FindObjectEnd(openBrace) - 1, openBrace) <= openBrace + 1;
}

// First occurrence of needle that starts before end, or NULL.
static char* FindBefore(char* start, char* end, const char* needle)
{
    char* hit = strstr(start, needle);
    if (!hit || hit >= end) return NULL;
    return hit;
}

// Finds "\"key\":" anywhere in the buffer (the game writes "key": value).
static char* FindKeyColon(char* buf, const char* key)
{
    char pattern[160];
    _snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    return strstr(buf, pattern);
}

// Object value of key inside the object starting at objOpen.
static char* FindChildObject(char* objOpen, const char* key)
{
    char* objEnd = FindObjectEnd(objOpen);
    char pattern[160];
    _snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* k = FindBefore(objOpen, objEnd, pattern);
    if (!k) return NULL;
    char* o = strchr(k, '{');
    if (!o || o >= objEnd) return NULL;
    return o;
}

// Array value of key inside the object starting at objOpen.
static char* FindChildArray(char* objOpen, const char* key)
{
    char* objEnd = FindObjectEnd(objOpen);
    char pattern[160];
    _snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* k = FindBefore(objOpen, objEnd, pattern);
    if (!k) return NULL;
    char* a = strchr(k, '[');
    if (!a || a >= objEnd) return NULL;
    return a;
}

// Appends value to the array at bracket unless it is already there. The caller
// must re-resolve bracket after every call because the buffer shifts.
static bool AppendArrayValue(char* buf, long cap, long* len, char* bracket, const char* value)
{
    if (*bracket != '[') return false;
    char* close = FindArrayEnd(bracket);
    if (!close || close[-1] != ']') return false;
    char needle[192];
    _snprintf(needle, sizeof(needle), "\"%s\"", value);
    if (FindBefore(bracket, close, needle)) return true;
    char* at = SkipBackWs(close - 1, bracket);
    char text[224];
    _snprintf(text, sizeof(text), "%s\n      \"%s\"\n    ", at <= bracket + 1 ? "" : ",", value);
    return InsertAt(buf, cap, len, at - buf, text);
}

// Adds values to the key of an object, creating the array when it is missing.
static bool EnsureArrayValues(char* buf, long cap, long* len, char* objOpen, const char* key, const char* const* values, int count)
{
    if (!FindChildArray(objOpen, key)) {
        char list[1024];
        int n = _snprintf(list, sizeof(list), "\"%s\": [", key);
        if (n <= 0 || n >= (int)sizeof(list)) return false;
        long used = n;
        for (int i = 0; i < count; i++) {
            n = _snprintf(list + used, sizeof(list) - used, "%s\"%s\"", i ? ", " : "", values[i]);
            if (n <= 0 || n >= (int)(sizeof(list) - used)) return false;
            used += n;
        }
        _snprintf(list + used, sizeof(list) - used, "]");
        char* objEnd = FindObjectEnd(objOpen);
        char* at = SkipBackWs(objEnd - 1, objOpen);
        char text[1100];
        _snprintf(text, sizeof(text), "%s\n      %s", IsEmptyObject(objOpen) ? "" : ",", list);
        return InsertAt(buf, cap, len, at - buf, text);
    }
    for (int i = 0; i < count; i++) {
        char* arr = FindChildArray(objOpen, key);
        if (!arr) return false;
        if (!AppendArrayValue(buf, cap, len, arr, values[i])) return false;
    }
    return true;
}

// Creates the DLC section of schedulerPartialDLC / schedulerPartial when the
// save predates that DLC, so unrelated player progress stays untouched.
static char* EnsureDlcBlock(char* buf, long cap, long* len, const char* dlcKey)
{
    const char* rootKey = dlcKey ? "schedulerPartialDLC" : "schedulerPartial";
    char* root = FindKeyColon(buf, rootKey);
    if (!root) return NULL;
    if (!dlcKey) return strchr(root, '{');
    char* rootOpen = strchr(root, '{');
    if (!rootOpen) return NULL;
    char* rootEnd = FindObjectEnd(rootOpen);
    char pattern[64];
    _snprintf(pattern, sizeof(pattern), "\"%s\":", dlcKey);
    char* k = FindBefore(rootOpen, rootEnd, pattern);
    if (k) return strchr(k, '{');
    char text[512];
    _snprintf(text, sizeof(text),
        "%s\n    \"%s\": {\n      \"dlcSaveDate\": 0,\n      \"scheduledEvents\": {},\n"
        "      \"scheduledNews\": {},\n      \"scheduledNewsReplaceContents\": {},\n"
        "      \"allTrackingMissions\": {},\n      \"finishedEvents\": [],\n      \"finishedMissions\": []\n    }",
        IsEmptyObject(rootOpen) ? "" : ",", dlcKey);
    char* at = SkipBackWs(rootEnd - 1, rootOpen);
    if (!InsertAt(buf, cap, len, at - buf, text)) return NULL;
    root = FindKeyColon(buf, rootKey);
    rootOpen = strchr(root, '{');
    rootEnd = FindObjectEnd(rootOpen);
    k = FindBefore(rootOpen, rootEnd, pattern);
    return k ? strchr(k, '{') : NULL;
}

static bool DlcActivated(char* buf, const char* dlcKey)
{
    if (!dlcKey) return true;
    char* key = FindKeyColon(buf, "allActivatedDLC");
    if (!key) return false;
    char* arr = strchr(key, '[');
    if (!arr) return false;
    char* end = FindArrayEnd(arr);
    char needle[64];
    _snprintf(needle, sizeof(needle), "\"%s\"", dlcKey);
    return FindBefore(arr, end, needle) != NULL;
}

struct BossUnlockDef
{
    const char* label;
    const char* methods;
    const char* desc;
    const char* dlcKey;
    const char* const* queueEvents;
    int queueCount;
    const char* const* clearEvents;
    int clearCount;
    const char* const* clearMissions;
    int clearMissionCount;
    const char* const* clearSwitches;
    int clearSwitchCount;
    bool hasInvite;
};

static const char* BOSS0_QUEUE[] = {"Challenge_Finale_P1"};
static const char* BOSS0_MISSIONS[] = {"Main_5_BambooForest_023_Mission"};

static const char* BOSS1_QUEUE[] = {
    "DLC1_Main_Toutetsu_First_RepeatChallenge_P1"
};
static const char* BOSS1_EVENTS[] = {
    "DLC1_Main_Toutetsu_004_Challange_Success"
};
static const char* BOSS1_MISSIONS[] = {"DLC1_Main_Toutetsu_004_Mission"};
static const char* BOSS1_SWITCHES[] = {"Kyouko_Tutorial_Toutetsu"};

static const char* BOSS2_QUEUE[] = {"DLC2_Main_FormerHell_WeirdCooking_Challenge_P1"};
static const char* BOSS2_MISSIONS[] = {"DLC2_Main_FormerHell_WeirdCooking_Mission_Enter"};

static const char* BOSS3_QUEUE[] = {"DLC3_Repeat_GobackHakureiShrine_Event"};
static const char* BOSS3_EVENTS[] = {
    "DLC3_MausoleumCuisineCompetition_Result_P1",
    "DLC3_MausoleumCuisineCompetition_Result_P2"
};
static const char* BOSS3_MISSIONS[] = {"DLC3_MausoleumCuisineCompetition_Mission"};

static const char* BOSS4_QUEUE[] = {"DLC4_Main_Part10_RepeatChallenge_Begin_Event"};
static const char* BOSS4_EVENTS[] = {
    "DLC4_Main_FlandreCabin_Enter_Event"
};
static const char* BOSS4_MISSIONS[] = {"DLC4_Main_Part10_Mission"};
static const char* BOSS4_SWITCHES[] = {"FirstTimeToSDMBasement"};

static const char* BOSS5_QUEUE[] = {"DLC5_RepeatChallenge_ArrestMizuchi_Enter_Event"};
static const char* BOSS5_EVENTS[] = {
    "DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event"
};
static const char* BOSS5_MISSIONS[] = {"DLC5_Challenge_ArrestMizuchi_Mission"};
static const char* BOSS5_SWITCHES[] = {"DLC5_Map_Makai_Portal", "DLC5_Makai_RestrictedZoneDoor"};

static const BossUnlockDef BOSS_DEFS[] = {
    {"与幽幽子的决战", "A+B",
     "方案A（已停用）：只往存档写 scheduledEvents 触发不了（已实测无效）。真正生效需要在游戏运行时调用它自己的 ScheduleEventExtern，等于要改内存/注入，暂未实现，留给有缘人。\n"
     "方案B：写入 finishedMissions。只把最终战标记为已完成，等于跳过，不会开打。",
     NULL, BOSS0_QUEUE, 1, NULL, 0, BOSS0_MISSIONS, 1, NULL, 0, false},
    {"饕餮挑战赛", "A+B",
     "方案A（已停用）：同下，只改存档触发不了，需运行时调用游戏自己的 ScheduleEventExtern。\n"
     "方案B：写 finishedEvents = DLC1_Main_Toutetsu_004_Challange_Success。之后到妖怪山找荷取对话，选「再战」。",
     "DLC1", BOSS1_QUEUE, 1, BOSS1_EVENTS, 1, BOSS1_MISSIONS, 1, BOSS1_SWITCHES, 1, false},
    {"怪诞料理挑战赛", "A+B+C",
     "方案A（已停用）：同下，只改存档触发不了，需运行时调用游戏自己的 ScheduleEventExtern。\n"
     "方案B：写 finishedMissions = DLC2_Main_FormerHell_WeirdCooking_Mission_Enter（阿燐的再战选项读任务完成数组）。之后找阿燐对话。\n"
     "方案C：添加邀请函（物品 2014~2019）刷好感，走原版路线。对应下方「方案C」按钮。",
     "DLC2", BOSS2_QUEUE, 1, NULL, 0, BOSS2_MISSIONS, 1, NULL, 0, true},
    {"博丽大祭", "A+B",
     "方案A（已停用）：同下，只改存档触发不了，需运行时调用游戏自己的 ScheduleEventExtern。\n"
     "方案B：写 finishedEvents（料理对决结果）。之后在游戏内开启博丽大祭，到神社找时焉侑选挑战。",
     "DLC3", BOSS3_QUEUE, 1, BOSS3_EVENTS, 2, BOSS3_MISSIONS, 1, NULL, 0, false},
    {"芙兰朵露挑战赛", "A+B",
     "方案A（已停用）：同下，只改存档触发不了，需运行时调用游戏自己的 ScheduleEventExtern。\n"
     "方案B：写 finishedEvents = DLC4_Main_FlandreCabin_Enter_Event。之后到芙兰的房间对话选「再战」。",
     "DLC4", BOSS4_QUEUE, 1, BOSS4_EVENTS, 1, BOSS4_MISSIONS, 1, BOSS4_SWITCHES, 1, false},
    {"瑞灵", "A+B",
     "方案A（已停用）：同下，只改存档触发不了，需运行时调用游戏自己的 ScheduleEventExtern。\n"
     "方案B：写 finishedEvents = DLC5_Challenge_ArrestMizuchi_Successful_GoHome_Event，并打开月都/魔界门开关。之后到月都控制台选再战。",
     "DLC5", BOSS5_QUEUE, 1, BOSS5_EVENTS, 1, BOSS5_MISSIONS, 1, BOSS5_SWITCHES, 2, false}
};

static const int BOSS_DEF_COUNT = (int)(sizeof(BOSS_DEFS) / sizeof(BOSS_DEFS[0]));

static const char* SWITCH_KEYS[] = {
    "Aya_FamousIzakaya",
    "Lantern_A_Display",
    "Lantern_B_Display",
    "Lantern_C_Display",
    "Lantern_D_Display",
    "Lantern_E_Display",
    "MengChengGuo",
    "3Faries",
    "DLC2.5_MusicMachine",
    "DLC3_Main_Part3_PalmCivet",
    "Kyouko_Tutorial_Top",
    "Kyouko_Tutorial_Preset",
    "Kyouko_Tutorial_Showcase",
    "Kyouko_Tutorial_Closet",
    "Kyouko_Tutorial_CDPlayer",
    "Kyouko_Tutorial_SpellCard",
    "Kyouko_Tutorial_Kourindou",
    "Kyouko_Tutorial_Hakugyokurou",
    "Kyouko_Tutorial_DLC",
    "Kyouko_Tutorial_ForDLC1MainStory",
    "HumanVillage_Farmland_A_Disabled",
    "HumanVillage_Farmland_B_Disabled",
    "HumanVillage_Farmland_C_Disabled",
    "DLC1_MagicForest_MagicTree_Green",
    "DLC5_Map_Makai_Portal",
    "Aunn_Stone",
    "DLC5_Main_Part6_Tenshi",
    "DLC3_HakureiFestival_RepeatChallenge_JienYuuCharacter",
    "DLC3_HakureiFestival_JienYuu",
    "Daiyousei_Ice",
    "Sakuya_Door",
    "TBC2_Collab_Has_Interact",
    "3FARIES_Collab_Has_Interact",
    "MC_Gensokyo_Has_Interact",
    "TBS_Kokoro",
    "TBS_Kokoro_Has_Interact",
    "TRACKED_SWITCH_RINNOSUKE_WELCOME",
    "TRACKED_SWITCH_RINNOSUKE_GETCOUPLE",
    "THYG_Has_Interact"
};

static const bool SWITCH_VALUES[] = {
    false, true, true, true, true, true, true, true, true, false,
    true, true, true, true, true, true, true, false, true, false,
    false, false, false, false, false, true, false, true, true, false,
    true, true, true, true, false, true, false, true, true
};

static const int SWITCH_KEY_COUNT = (int)(sizeof(SWITCH_KEYS) / sizeof(SWITCH_KEYS[0]));

static bool SetTrackedSwitch(char* buf, long cap, long* len, char* tsKey, const char* key, bool value)
{
    if (!tsKey) return false;
    char* objOpen = strchr(tsKey, '{');
    if (!objOpen) return false;
    char* objEnd = FindObjectEnd(objOpen);
    char pattern[160];
    _snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* k = FindBefore(objOpen, objEnd, pattern);
    if (k) {
        char* colon = strchr(k, ':');
        if (!colon || colon >= objEnd) return false;
        char* v = colon + 1;
        while (*v == ' ' && v < objEnd) v++;
        bool current = (strncmp(v, "true", 4) == 0);
        if (current == value) return true;
        ReplaceRange(buf, cap, len, v - buf, current ? 4 : 5, value ? "true" : "false");
        return true;
    }
    char* at = SkipBackWs(objEnd - 1, objOpen);
    char text[192];
    _snprintf(text, sizeof(text), "%s\n      \"%s\": %s", at <= objOpen + 1 ? "" : ",", key, value ? "true" : "false");
    return InsertAt(buf, cap, len, at - buf, text);
}

int SaveEditor_GetBossCount(void)
{
    return BOSS_DEF_COUNT;
}

const char* SaveEditor_GetBossLabel(int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return "";
    return BOSS_DEFS[bossId].label;
}

const char* SaveEditor_GetBossMethods(int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return "";
    return BOSS_DEFS[bossId].methods;
}

const char* SaveEditor_GetBossDesc(int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return "";
    return BOSS_DEFS[bossId].desc;
}

static const bool QUEUE_METHOD_ENABLED = false;

int SaveEditor_BossHasQueue(int bossId)
{
    if (!QUEUE_METHOD_ENABLED) return 0;
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return 0;
    return BOSS_DEFS[bossId].queueCount > 0 ? 1 : 0;
}

int SaveEditor_BossHasClear(int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return 0;
    const BossUnlockDef& def = BOSS_DEFS[bossId];
    return (def.clearCount > 0 || def.clearMissionCount > 0 || def.clearSwitchCount > 0) ? 1 : 0;
}

int SaveEditor_BossHasInvite(int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return 0;
    return BOSS_DEFS[bossId].hasInvite ? 1 : 0;
}

static int GetNextDay(char* buf, char* keyOut, int keySize)
{
    char* gd = FindKeyColon(buf, "gameDate");
    if (!gd) return 0;
    char* obj = strchr(gd, '{');
    if (!obj) return 0;
    char* end = FindObjectEnd(obj);
    char* dk = FindBefore(obj, end, "\"day\":");
    if (!dk) return 0;
    char* colon = strchr(dk, ':');
    if (!colon || colon >= end) return 0;
    char* v = colon + 1;
    while (v < end && (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n')) v++;
    if (v >= end || *v < '0' || *v > '9') return 0;
    int day = 0;
    while (v < end && *v >= '0' && *v <= '9') { day = day * 10 + (*v - '0'); v++; }
    _snprintf(keyOut, keySize, "%d", day + 1);
    return 1;
}

int SaveEditor_QueueBossEvents(const char* path, int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return -3;
    const BossUnlockDef& def = BOSS_DEFS[bossId];
    if (def.queueCount <= 0) return -6;

    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;
    long cap = len + FILE_SLACK;
    int ret = -7;

    if (!DlcActivated(buf, def.dlcKey)) {
        free(buf);
        return -5;
    }
    char dayKey[32];
    if (!GetNextDay(buf, dayKey, sizeof(dayKey)))
        strcpy(dayKey, "-1");

    char* block = EnsureDlcBlock(buf, cap, &len, def.dlcKey);
    char* scheduled = block ? FindChildObject(block, "scheduledEvents") : NULL;
    if (scheduled && EnsureArrayValues(buf, cap, &len, scheduled, dayKey, def.queueEvents, def.queueCount))
        ret = SaveEditedFile(path, buf, len);

    free(buf);
    return ret;
}

int SaveEditor_SetBossCleared(const char* path, int bossId)
{
    if (bossId < 0 || bossId >= BOSS_DEF_COUNT) return -3;
    const BossUnlockDef& def = BOSS_DEFS[bossId];

    long len = 0;
    char* buf = LoadFileForEdit(path, &len);
    if (!buf) return (int)len;
    long cap = len + FILE_SLACK;
    int ret = -7;

    if (!DlcActivated(buf, def.dlcKey)) {
        free(buf);
        return -5;
    }
    char* block = EnsureDlcBlock(buf, cap, &len, def.dlcKey);
    if (block) {
        bool ok = true;
        if (def.clearCount > 0)
            ok = EnsureArrayValues(buf, cap, &len, block, "finishedEvents", def.clearEvents, def.clearCount);
        if (ok && def.clearMissionCount > 0) {
            block = EnsureDlcBlock(buf, cap, &len, def.dlcKey);
            ok = block && EnsureArrayValues(buf, cap, &len, block, "finishedMissions", def.clearMissions, def.clearMissionCount);
        }
        if (ok && def.clearSwitchCount > 0) {
            char* tsKey = FindKeyColon(buf, "trackedSwitch");
            for (int i = 0; i < def.clearSwitchCount && ok; i++)
                ok = SetTrackedSwitch(buf, cap, &len, tsKey, def.clearSwitches[i], true);
        }
        if (ok) ret = SaveEditedFile(path, buf, len);
    }

    free(buf);
    return ret;
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
        // Whitespace must be skipped too: an empty items object may be written
        // as "{ }" instead of "{}".
        char* at = SkipBackWs(buf + insert_pos, brace);
        int has_comma = (at > brace + 1);
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
                    len += SetBondEntries(brace, &end, DLC3_BOND_IDS, BOND_ID_COUNT, DLC3_BOND_EXP_VALUE);
                }
            }
        }
    }

    // 2. Merge trackedSwitch, keeping switches this tool does not know about
    char* ts = FindKeyColon(buf, "trackedSwitch");
    if (ts) {
        for (int i = 0; i < SWITCH_KEY_COUNT; i++) {
            if (!SetTrackedSwitch(buf, cap, &len, ts, SWITCH_KEYS[i], SWITCH_VALUES[i])) {
                free(buf);
                return -2;
            }
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

    char* player = FindKeyColon(buf, "playerPartial");
    if (!player) { free(buf); return -3; }
    char* playerOpen = strchr(player, '{');
    if (!playerOpen) { free(buf); return -3; }
    char* playerEnd = FindObjectEnd(playerOpen);
    // Scoped to playerPartial: a bare "fund" may also match an unrelated key.
    char* fund = FindBefore(playerOpen, playerEnd, "\"fund\"");
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
