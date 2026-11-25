#include "features.h"
#include "game_api.h"
#include "il2cpp_api.h"
#include "log.h"
#include "MinHook.h"
#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

namespace
{
const char* kGameImage = "Assembly-CSharp.dll";
const char* kUnityImage = "UnityEngine.CoreModule.dll";

enum FeatureId
{
    F_FREEZE_DAY = 0,
    F_FREEZE_NIGHT,
    F_MAX_COMBO,
    F_LOCK_PATIENCE,
    F_MAX_MOOD,
    F_INSTANT_COOK,
    F_PERFECT_QTE,
    F_INFINITE_EXP,
    F_MONEY_MULT,
    F_PICKUP_MULT,
    F_EXP_MULT,
    F_MOVE_SPEED,
    F_GAME_SPEED,
    F_COUNT
};

struct FeatureDef
{
    const char* key;
    FeatureId id;
    bool hasValue;
    float defaultValue;
};

const FeatureDef kFeatures[] = {
    {"freeze_day", F_FREEZE_DAY, false, 0.0f},
    {"freeze_night", F_FREEZE_NIGHT, false, 0.0f},
    {"max_combo", F_MAX_COMBO, false, 0.0f},
    {"lock_patience", F_LOCK_PATIENCE, false, 0.0f},
    {"max_mood", F_MAX_MOOD, false, 0.0f},
    {"instant_cook", F_INSTANT_COOK, false, 0.0f},
    {"perfect_qte", F_PERFECT_QTE, false, 0.0f},
    {"infinite_exp", F_INFINITE_EXP, false, 0.0f},
    {"money_multiplier", F_MONEY_MULT, true, 2.0f},
    {"pickup_multiplier", F_PICKUP_MULT, true, 2.0f},
    {"exp_multiplier", F_EXP_MULT, true, 2.0f},
    {"move_speed_multiplier", F_MOVE_SPEED, true, 2.0f},
    {"game_speed", F_GAME_SPEED, true, 2.0f},
};

std::atomic<bool> g_on[F_COUNT];
std::atomic<bool> g_installed[F_COUNT];
std::atomic<float> g_value[F_COUNT];
std::atomic<bool> g_infiniteArmed(false);
std::atomic<bool> g_skipItemMultiplier(false);
std::atomic<int> g_moveReset(0);
std::atomic<float> g_baseTimeScale(1.0f);
thread_local bool tl_internalTimeSet = false;
bool g_minHookReady = false;

int g_offProductType = -1;
int g_offProductAmount = -1;
int g_offShouldTimeFlow = -1;
int g_offCurrentCombo = -1;
int g_offMood = -1;
int g_offNoteStatus = -1;
int g_offMoveMultiplier = -1;
int g_offDayCharacter = -1;
int g_offWorkCharacter = -1;

void* g_getTimeScalePtr = NULL;
void* g_setTimeScaleRaw = NULL;
typedef void (*AddFundFn)(int, bool);
typedef void (*SendToStorageFn)(void*, bool);
typedef int (*RealTimeCountDownFn)(void*);
typedef bool (*BoolGetterFn)(void*);
typedef void (*SetMoodProgressFn)(void*, float);
typedef void (*UpdatePatientFn)(void*);
typedef float (*CookSpeedFn)(void*, void*, void*);
typedef void (*EvaluateNoteFn)(void*, void*);
typedef void (*AddExpFn)(int);
typedef void (*WarpActionsFn)(int, void*);
typedef void (*UpdateFn)(void*);
typedef void (*SetTimeScaleFn)(float);
typedef float (*GetTimeScaleFn)();

AddFundFn g_addFundOrig = NULL;
SendToStorageFn g_sendToStorageOrig = NULL;
RealTimeCountDownFn g_realTimeCountDownOrig = NULL;
BoolGetterFn g_shouldGuestPatientFreezeOrig = NULL;
SetMoodProgressFn g_setMoodProgressOrig = NULL;
UpdatePatientFn g_updatePatientOrig = NULL;
CookSpeedFn g_getExtraCookSpeedMultiplierOrig = NULL;
EvaluateNoteFn g_evaluateAndReleaseNoteOrig = NULL;
AddExpFn g_addExpOrig = NULL;
WarpActionsFn g_warpActionsOrig = NULL;
UpdateFn g_dayUpdateOrig = NULL;
UpdateFn g_workFixedUpdateOrig = NULL;
SetTimeScaleFn g_setTimeScaleOrig = NULL;

const FeatureDef* FindFeature(const std::string& key)
{
    for (size_t i = 0; i < sizeof(kFeatures) / sizeof(kFeatures[0]); i++) {
        if (key == kFeatures[i].key) return &kFeatures[i];
    }
    return NULL;
}

const char* KeyOf(FeatureId id)
{
    for (size_t i = 0; i < sizeof(kFeatures) / sizeof(kFeatures[0]); i++) {
        if (kFeatures[i].id == id) return kFeatures[i].key;
    }
    return "?";
}

bool ResolveMethod(const char* ns, const char* cls, const char* method, int argc,
                   bool expectStatic, void** info, void** ptr, std::string* error)
{
    void* mi = Il2Cpp_Method(kGameImage, ns, cls, method, argc);
    if (!mi) {
        if (error) *error = std::string("未找到 ") + ns + "." + cls + "." + method;
        return false;
    }
    if (Il2Cpp_MethodIsStatic(mi) != expectStatic) {
        if (error) *error = std::string("静态属性不符 ") + cls + "." + method;
        return false;
    }
    void* p = Il2Cpp_MethodPointer(mi);
    if (!p) {
        if (error) *error = std::string("方法指针为空 ") + cls + "." + method;
        return false;
    }
    if (info) *info = mi;
    if (ptr) *ptr = p;
    return true;
}

int ResolveField(const char* ns, const char* cls, std::initializer_list<const char*> names)
{
    for (const char* name : names) {
        void* field = Il2Cpp_Field(kGameImage, ns, cls, name);
        if (field) return Il2Cpp_FieldOffset(field);
    }
    return -1;
}

template <typename T>
T ReadAt(void* base, int offset)
{
    T value = T();
    if (!base || offset < 0) return value;
    memcpy(&value, (char*)base + offset, sizeof(T));
    return value;
}

template <typename T>
void WriteAt(void* base, int offset, T value)
{
    if (!base || offset < 0) return;
    memcpy((char*)base + offset, &value, sizeof(T));
}

int ScaleInt(int value, float multiplier, int limit)
{
    double scaled = (double)value * (double)multiplier;
    if (scaled > (double)limit) scaled = (double)limit;
    if (scaled < -(double)limit) scaled = -(double)limit;
    return scaled >= 0.0 ? (int)(scaled + 0.5) : (int)(scaled - 0.5);
}

float ClampMultiplier(float value)
{
    if (!(value > 0.0f)) return 2.0f;
    if (value < 0.1f) return 0.1f;
    if (value > 10.0f) return 10.0f;
    return value;
}

void ApplyTimeScale()
{
    if (!g_setTimeScaleOrig) return;
    if (Game_IsFrozen()) return;

    float base = g_baseTimeScale.load(std::memory_order_relaxed);
    float target = g_on[F_GAME_SPEED].load(std::memory_order_relaxed)
        ? base * g_value[F_GAME_SPEED].load(std::memory_order_relaxed)
        : base;
    if (!(target >= 0.0f)) target = 0.0f;

    tl_internalTimeSet = true;
    g_setTimeScaleOrig(target);
    tl_internalTimeSet = false;
}

void Hook_AddFund(int amount, bool show)
{
    if (g_on[F_MONEY_MULT].load(std::memory_order_relaxed) && amount > 0) {
        amount = ScaleInt(amount, g_value[F_MONEY_MULT].load(std::memory_order_relaxed), 2000000000);
    }
    g_addFundOrig(amount, show);
}

void Hook_SendToStorage(void* product, bool show)
{
    if (g_on[F_PICKUP_MULT].load(std::memory_order_relaxed) && product &&
        !g_skipItemMultiplier.load(std::memory_order_relaxed) &&
        g_offProductType >= 0 && g_offProductAmount >= 0) {
        int type = ReadAt<int>(product, g_offProductType);
        if (type == 0 || type == 1 || type == 2 || type == 3 || type == 5) {
            int amount = ReadAt<int>(product, g_offProductAmount);
            if (amount > 0 && amount < 99999) {
                float multiplier = g_value[F_PICKUP_MULT].load(std::memory_order_relaxed);
                WriteAt<int>(product, g_offProductAmount, ScaleInt(amount, multiplier, 99999));
            }
        }
    }
    g_sendToStorageOrig(product, show);
}

int Hook_RealTimeCountDown(void* self)
{
    if (self) {
        if (g_on[F_FREEZE_NIGHT].load(std::memory_order_relaxed) && g_offShouldTimeFlow >= 0) {
            WriteAt<unsigned char>(self, g_offShouldTimeFlow, 0);
        }
        if (g_on[F_MAX_COMBO].load(std::memory_order_relaxed) && g_offCurrentCombo >= 0) {
            if (ReadAt<int>(self, g_offCurrentCombo) < 899) {
                WriteAt<int>(self, g_offCurrentCombo, 899);
            }
        }
    }
    return g_realTimeCountDownOrig(self);
}

bool Hook_ShouldGuestPatientFreeze(void* self)
{
    if (g_on[F_LOCK_PATIENCE].load(std::memory_order_relaxed)) return true;
    return g_shouldGuestPatientFreezeOrig(self);
}

void Hook_SetMoodProgress(void* self, float progress)
{
    if (g_on[F_MAX_MOOD].load(std::memory_order_relaxed)) progress = 1.0f;
    g_setMoodProgressOrig(self, progress);
}

void Hook_UpdatePatient(void* self)
{
    if (g_on[F_MAX_MOOD].load(std::memory_order_relaxed)) {
        WriteAt<int>(self, g_offMood, 100);
    }
    g_updatePatientOrig(self);
}

float Hook_GetExtraCookSpeedMultiplier(void* self, void* cookController, void* sellable)
{
    if (g_on[F_INSTANT_COOK].load(std::memory_order_relaxed)) return 0.0001f;
    return g_getExtraCookSpeedMultiplierOrig(self, cookController, sellable);
}

void Hook_EvaluateAndReleaseNote(void* self, void* note)
{
    if (g_on[F_PERFECT_QTE].load(std::memory_order_relaxed)) {
        WriteAt<int>(note, g_offNoteStatus, 1);
    }
    g_evaluateAndReleaseNoteOrig(self, note);
}

void Hook_AddExp(int exp)
{
    if (exp > 0) {
        if (g_on[F_EXP_MULT].load(std::memory_order_relaxed)) {
            exp = ScaleInt(exp, g_value[F_EXP_MULT].load(std::memory_order_relaxed), 100000000);
        }
        if (g_infiniteArmed.exchange(false, std::memory_order_relaxed)) {
            exp = 9999999;
        }
    }
    g_addExpOrig(exp);
}

void Hook_WarpActions(int delta, void* callback)
{
    if (g_on[F_FREEZE_DAY].load(std::memory_order_relaxed) && delta < 5) delta = 0;
    g_warpActionsOrig(delta, callback);
}

void ApplyMoveSpeed(void* generator, int characterOffset)
{
    if (!generator || characterOffset < 0 || g_offMoveMultiplier < 0) return;
    void* character = ReadAt<void*>(generator, characterOffset);
    if (!character) return;

    if (g_on[F_MOVE_SPEED].load(std::memory_order_relaxed)) {
        WriteAt<float>(character, g_offMoveMultiplier, g_value[F_MOVE_SPEED].load(std::memory_order_relaxed));
        g_moveReset.store(0, std::memory_order_relaxed);
    } else if (g_moveReset.exchange(0, std::memory_order_relaxed) != 0) {
        WriteAt<float>(character, g_offMoveMultiplier, 1.0f);
    }
}

void Hook_DayUpdate(void* self)
{
    ApplyMoveSpeed(self, g_offDayCharacter);
    g_dayUpdateOrig(self);
}

void Hook_WorkFixedUpdate(void* self)
{
    ApplyMoveSpeed(self, g_offWorkCharacter);
    g_workFixedUpdateOrig(self);
}

void Hook_SetTimeScale(float value)
{
    if (tl_internalTimeSet) {
        g_setTimeScaleOrig(value);
        return;
    }
    if (value > 0.0f) {
        g_baseTimeScale.store(value, std::memory_order_relaxed);
    }
    float out = value;
    if (value > 0.0f && g_on[F_GAME_SPEED].load(std::memory_order_relaxed)) {
        out = value * g_value[F_GAME_SPEED].load(std::memory_order_relaxed);
    }
    g_setTimeScaleOrig(out);
}

bool CreateHook(const char* name, void* target, void* detour, void** original)
{
    MH_STATUS status = MH_CreateHook(target, detour, (LPVOID*)original);
    if (status != MH_OK) {
        CoreLog("hook %s create failed: %d", name, (int)status);
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK) {
        CoreLog("hook %s enable failed: %d", name, (int)status);
        MH_RemoveHook(target);
        if (original) *original = NULL;
        return false;
    }
    return true;
}

void MarkReady(FeatureId id, bool ready)
{
    g_installed[id].store(ready, std::memory_order_relaxed);
    if (!ready) CoreLog("feature %s unavailable", KeyOf(id));
}

void InitState()
{
    for (int i = 0; i < F_COUNT; i++) {
        g_on[i].store(false, std::memory_order_relaxed);
        g_installed[i].store(false, std::memory_order_relaxed);
        g_value[i].store(0.0f, std::memory_order_relaxed);
    }
    for (size_t i = 0; i < sizeof(kFeatures) / sizeof(kFeatures[0]); i++) {
        g_value[kFeatures[i].id].store(kFeatures[i].defaultValue, std::memory_order_relaxed);
    }
    g_value[F_GAME_SPEED].store(2.0f, std::memory_order_relaxed);
    g_value[F_MOVE_SPEED].store(2.0f, std::memory_order_relaxed);
    g_infiniteArmed.store(false, std::memory_order_relaxed);
    g_skipItemMultiplier.store(false, std::memory_order_relaxed);
    g_moveReset.store(0, std::memory_order_relaxed);
    g_baseTimeScale.store(1.0f, std::memory_order_relaxed);
}
}

bool Features_IsKnown(const std::string& key)
{
    return FindFeature(key) != NULL;
}

bool Features_Ready(const std::string& key)
{
    const FeatureDef* def = FindFeature(key);
    return def && g_installed[def->id].load(std::memory_order_relaxed);
}

bool Features_Init(std::string* error)
{
    if (!g_minHookReady) {
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            if (error) *error = "MinHook 初始化失败 " + std::to_string((int)status);
            CoreLog("minhook init failed: %d", (int)status);
            return false;
        }
        g_minHookReady = true;
    }

    static bool initialized = false;
    if (!initialized) {
        InitState();
        initialized = true;
    }

    g_offProductType = ResolveField("GameData.Core.Collections.DaySceneUtility.Collections", "Product",
                                    {"productType", "ProductType"});
    g_offProductAmount = ResolveField("GameData.Core.Collections.DaySceneUtility.Collections", "Product",
                                      {"productAmount", "ProductAmount"});
    g_offShouldTimeFlow = ResolveField("NightScene.EventUtility", "EventManager",
                                       {"<ShouldTimeFlow>k__BackingField", "_ShouldTimeFlow_k__BackingField",
                                        "ShouldTimeFlow"});
    g_offCurrentCombo = ResolveField("NightScene.EventUtility", "EventManager",
                                     {"<CurrentCombo>k__BackingField", "_CurrentCombo_k__BackingField",
                                      "CurrentCombo", "combo"});
    g_offMood = ResolveField("NightScene.GuestManagementUtility", "GuestGroupController",
                             {"mood", "_mood", "<Mood>k__BackingField"});
    g_offNoteStatus = ResolveField("Common.UI.QTEUtility", "NoteHeadBase", {"m_Status", "Status"});
    g_offMoveMultiplier = ResolveField("Common.CharacterUtility", "CharacterControllerUnit",
                                       {"<MoveSpeedMultiplier>k__BackingField",
                                        "_MoveSpeedMultiplier_k__BackingField",
                                        "MoveSpeedMultiplier", "m_MoveSpeedMultiplier"});
    g_offDayCharacter = ResolveField("DayScene.Input", "DayScenePlayerInputGenerator",
                                     {"<Character>k__BackingField", "_Character_k__BackingField",
                                      "Character", "m_Character"});
    g_offWorkCharacter = ResolveField("NightScene.Input", "WorkScenePlayerInputGenerator",
                                      {"<Character>k__BackingField", "_Character_k__BackingField",
                                       "Character", "m_Character"});

    CoreLog("feature offsets product=%d/%d timeFlow=%d combo=%d mood=%d note=%d move=%d char=%d/%d",
            g_offProductType, g_offProductAmount, g_offShouldTimeFlow, g_offCurrentCombo,
            g_offMood, g_offNoteStatus, g_offMoveMultiplier, g_offDayCharacter, g_offWorkCharacter);

    std::string resolveError;
    void* addFundPtr = NULL;
    if (ResolveMethod("GameData.RunTime.Common", "RunTimePlayerData", "AddFund", 2, true,
                      NULL, &addFundPtr, &resolveError) &&
        CreateHook("AddFund", addFundPtr, (void*)Hook_AddFund, (void**)&g_addFundOrig)) {
        MarkReady(F_MONEY_MULT, true);
    } else {
        MarkReady(F_MONEY_MULT, false);
    }

    resolveError.clear();
    void* sendPtr = NULL;
    if (g_offProductType >= 0 && g_offProductAmount >= 0 &&
        ResolveMethod("GameData.RunTime.Common", "RunTimeStorage", "SendToStorage", 2, true,
                      NULL, &sendPtr, &resolveError) &&
        CreateHook("SendToStorage", sendPtr, (void*)Hook_SendToStorage, (void**)&g_sendToStorageOrig)) {
        MarkReady(F_PICKUP_MULT, true);
    } else {
        MarkReady(F_PICKUP_MULT, false);
    }

    resolveError.clear();
    void* realTimePtr = NULL;
    bool realTimeHooked = ResolveMethod("NightScene.EventUtility", "EventManager",
                                        "get_RealTimeCountDown", 0, false, NULL, &realTimePtr,
                                        &resolveError) &&
                         CreateHook("get_RealTimeCountDown", realTimePtr, (void*)Hook_RealTimeCountDown,
                                    (void**)&g_realTimeCountDownOrig);
    MarkReady(F_FREEZE_NIGHT, realTimeHooked && g_offShouldTimeFlow >= 0);
    MarkReady(F_MAX_COMBO, realTimeHooked && g_offCurrentCombo >= 0);

    resolveError.clear();
    void* patientPtr = NULL;
    bool patientHooked = ResolveMethod("NightScene.EventUtility", "EventManager",
                                       "get_ShouldGuestPatientFreeze", 0, false, NULL, &patientPtr,
                                       &resolveError) &&
                        CreateHook("get_ShouldGuestPatientFreeze", patientPtr,
                                   (void*)Hook_ShouldGuestPatientFreeze,
                                   (void**)&g_shouldGuestPatientFreezeOrig);
    MarkReady(F_LOCK_PATIENCE, patientHooked);

    resolveError.clear();
    void* updatePatientPtr = NULL;
    bool updatePatientHooked = g_offMood >= 0 &&
        ResolveMethod("NightScene.GuestManagementUtility", "GuestGroupController", "UpdatePatient",
                      0, false, NULL, &updatePatientPtr, &resolveError) &&
        CreateHook("UpdatePatient", updatePatientPtr, (void*)Hook_UpdatePatient,
                   (void**)&g_updatePatientOrig);
    resolveError.clear();
    void* setMoodPtr = NULL;
    bool setMoodHooked = ResolveMethod("NightScene.GuestManagementUtility", "GuestTableDisplayer",
                                       "SetMoodProgress", 1, false, NULL, &setMoodPtr, &resolveError) &&
                         CreateHook("SetMoodProgress", setMoodPtr, (void*)Hook_SetMoodProgress,
                                    (void**)&g_setMoodProgressOrig);
    MarkReady(F_MAX_MOOD, updatePatientHooked && setMoodHooked);

    resolveError.clear();
    void* cookPtr = NULL;
    bool cookHooked = ResolveMethod("GameData.Core.Collections", "Cooker",
                                    "GetExtraCookSpeedMultiplier", 2, false, NULL, &cookPtr,
                                    &resolveError) &&
                      CreateHook("GetExtraCookSpeedMultiplier", cookPtr,
                                 (void*)Hook_GetExtraCookSpeedMultiplier,
                                 (void**)&g_getExtraCookSpeedMultiplierOrig);
    MarkReady(F_INSTANT_COOK, cookHooked);

    resolveError.clear();
    void* notePtr = NULL;
    bool noteHooked = g_offNoteStatus >= 0 &&
        ResolveMethod("Common.UI.QTEUtility", "QTEPannelBase", "EvaluateAndReleaseNote", 1,
                      false, NULL, &notePtr, &resolveError) &&
        CreateHook("EvaluateAndReleaseNote", notePtr, (void*)Hook_EvaluateAndReleaseNote,
                   (void**)&g_evaluateAndReleaseNoteOrig);
    MarkReady(F_PERFECT_QTE, noteHooked);

    resolveError.clear();
    void* addExpPtr = NULL;
    bool expHooked = ResolveMethod("GameData.RunTime.Common", "RunTimePlayerData", "AddExp", 1,
                                   true, NULL, &addExpPtr, &resolveError) &&
                     CreateHook("AddExp", addExpPtr, (void*)Hook_AddExp, (void**)&g_addExpOrig);
    MarkReady(F_INFINITE_EXP, expHooked);
    MarkReady(F_EXP_MULT, expHooked);

    resolveError.clear();
    void* warpPtr = NULL;
    bool warpHooked = ResolveMethod("GameData.RunTime.DaySceneUtility", "RunTimeDayScene",
                                    "WarpActions", 2, true, NULL, &warpPtr, &resolveError) &&
                      CreateHook("WarpActions", warpPtr, (void*)Hook_WarpActions,
                                 (void**)&g_warpActionsOrig);
    MarkReady(F_FREEZE_DAY, warpHooked);

    resolveError.clear();
    void* dayUpdatePtr = NULL;
    bool dayUpdateHooked = g_offDayCharacter >= 0 && g_offMoveMultiplier >= 0 &&
        ResolveMethod("DayScene.Input", "DayScenePlayerInputGenerator", "Update", 0, false,
                      NULL, &dayUpdatePtr, &resolveError) &&
        CreateHook("DayScenePlayerInputGenerator.Update", dayUpdatePtr, (void*)Hook_DayUpdate,
                   (void**)&g_dayUpdateOrig);
    resolveError.clear();
    void* workUpdatePtr = NULL;
    bool workUpdateHooked = g_offWorkCharacter >= 0 && g_offMoveMultiplier >= 0 &&
        ResolveMethod("NightScene.Input", "WorkScenePlayerInputGenerator", "FixedUpdate", 0,
                      false, NULL, &workUpdatePtr, &resolveError) &&
        CreateHook("WorkScenePlayerInputGenerator.FixedUpdate", workUpdatePtr,
                   (void*)Hook_WorkFixedUpdate, (void**)&g_workFixedUpdateOrig);
    MarkReady(F_MOVE_SPEED, dayUpdateHooked && workUpdateHooked);

    resolveError.clear();
    void* getTimeScaleInfo = Il2Cpp_Method(kUnityImage, "UnityEngine", "Time", "get_timeScale", 0);
    void* setTimeScaleInfo = Il2Cpp_Method(kUnityImage, "UnityEngine", "Time", "set_timeScale", 1);
    void* setTimeScalePtr = setTimeScaleInfo ? Il2Cpp_MethodPointer(setTimeScaleInfo) : NULL;
    g_getTimeScalePtr = getTimeScaleInfo ? Il2Cpp_MethodPointer(getTimeScaleInfo) : NULL;
    g_setTimeScaleRaw = setTimeScalePtr;
    if (g_getTimeScalePtr) {
        float base = ((GetTimeScaleFn)g_getTimeScalePtr)();
        if (base > 0.0f) g_baseTimeScale.store(base, std::memory_order_relaxed);
    }
    bool timeHooked = g_getTimeScalePtr && setTimeScalePtr &&
        CreateHook("Time.set_timeScale", setTimeScalePtr, (void*)Hook_SetTimeScale,
                   (void**)&g_setTimeScaleOrig);
    MarkReady(F_GAME_SPEED, timeHooked);

    CoreLog("features ready: %s", Features_Readiness().c_str());
    if (error) error->clear();
    return true;
}

bool Features_Set(const std::string& key, bool enabled, float value, std::string* error)
{
    const FeatureDef* def = FindFeature(key);
    if (!def) {
        if (error) *error = "未知开关 " + key;
        return false;
    }
    if (!g_installed[def->id].load(std::memory_order_relaxed)) {
        if (error) *error = std::string("功能未就绪 ") + key;
        return false;
    }

    if (def->hasValue) {
        float next = ClampMultiplier(value);
        g_value[def->id].store(next, std::memory_order_relaxed);
    }

    if (def->id == F_GAME_SPEED) {
        g_on[F_GAME_SPEED].store(enabled, std::memory_order_relaxed);
        ApplyTimeScale();
        return true;
    }

    if (def->id == F_MOVE_SPEED && !enabled) {
        g_moveReset.store(1, std::memory_order_relaxed);
    }

    if (def->id == F_INFINITE_EXP) {
        g_infiniteArmed.store(enabled, std::memory_order_relaxed);
    }

    g_on[def->id].store(enabled, std::memory_order_relaxed);
    return true;
}

void Features_DisableAll()
{
    for (int i = 0; i < F_COUNT; i++) {
        g_on[i].store(false, std::memory_order_relaxed);
    }
    g_infiniteArmed.store(false, std::memory_order_relaxed);
    g_moveReset.store(1, std::memory_order_relaxed);
    ApplyTimeScale();
}

float Features_UnscaleTimeScale(float current)
{
    if (g_on[F_GAME_SPEED].load(std::memory_order_relaxed)) {
        return g_baseTimeScale.load(std::memory_order_relaxed);
    }
    return current;
}

bool Features_SetBaseTimeScale(float value, std::string* error)
{
    if (!(value > 0.0f)) value = 1.0f;
    if (!g_setTimeScaleOrig && !g_setTimeScaleRaw) {
        if (error) *error = "时间控制方法未就绪";
        return false;
    }
    g_baseTimeScale.store(value, std::memory_order_relaxed);
    if (Game_IsFrozen()) return true;

    float target = g_on[F_GAME_SPEED].load(std::memory_order_relaxed)
        ? value * g_value[F_GAME_SPEED].load(std::memory_order_relaxed)
        : value;
    tl_internalTimeSet = true;
    if (g_setTimeScaleOrig) {
        g_setTimeScaleOrig(target);
    } else {
        ((SetTimeScaleFn)g_setTimeScaleRaw)(target);
    }
    tl_internalTimeSet = false;
    return true;
}

std::string Features_Readiness()
{
    std::string out;
    for (size_t i = 0; i < sizeof(kFeatures) / sizeof(kFeatures[0]); i++) {
        if (!out.empty()) out += " ";
        out += kFeatures[i].key;
        out += g_installed[kFeatures[i].id].load(std::memory_order_relaxed) ? "=1" : "=0";
    }
    return out;
}

std::string Features_States()
{
    std::string out;
    for (size_t i = 0; i < sizeof(kFeatures) / sizeof(kFeatures[0]); i++) {
        if (!out.empty()) out += " ";
        out += kFeatures[i].key;
        out += g_on[kFeatures[i].id].load(std::memory_order_relaxed) ? "=1" : "=0";
    }
    return out;
}

std::string Features_Values()
{
    std::string out;
    for (size_t i = 0; i < sizeof(kFeatures) / sizeof(kFeatures[0]); i++) {
        if (!kFeatures[i].hasValue) continue;
        if (!out.empty()) out += " ";
        char value[40] = {0};
        _snprintf(value, sizeof(value) - 1, "%.4f", g_value[kFeatures[i].id].load(std::memory_order_relaxed));
        out += kFeatures[i].key;
        out += "=";
        out += value;
    }
    return out;
}

void Features_OnObtainBegin()
{
    g_skipItemMultiplier.store(true, std::memory_order_relaxed);
}

void Features_OnObtainEnd()
{
    g_skipItemMultiplier.store(false, std::memory_order_relaxed);
}
