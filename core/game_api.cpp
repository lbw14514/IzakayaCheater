#include "game_api.h"
#include "features.h"
#include "il2cpp_api.h"
#include "log.h"
#include "main_thread.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

namespace
{
bool g_resolved = false;
bool g_moneyReady = false;
bool g_speedReady = false;
bool g_featuresInitAttempted = false;

void* g_getFund = NULL;
void* g_setFund = NULL;
void* g_getTimeScale = NULL;
void* g_setTimeScale = NULL;
std::atomic<bool> g_frozen(false);
std::atomic<float> g_savedTimeScale(1.0f);

const char* kGameImage = "Assembly-CSharp.dll";
const char* kUnityImage = "UnityEngine.CoreModule.dll";

struct GcGuard
{
    GcGuard() { Il2Cpp_GcDisable(); }
    ~GcGuard() { Il2Cpp_GcEnable(); }
};

struct ObtainGuard
{
    ObtainGuard() { Features_OnObtainBegin(); }
    ~ObtainGuard() { Features_OnObtainEnd(); }
};

bool ResolveMethod(const char* image, const char* ns, const char* cls, const char* method,
                   int argc, bool expectStatic, void** out, std::string* error)
{
    void* mi = Il2Cpp_Method(image, ns, cls, method, argc);
    if (!mi) {
        if (error) *error = std::string("未找到 ") + ns + "." + cls + "." + method;
        return false;
    }
    if (Il2Cpp_MethodIsStatic(mi) != expectStatic) {
        if (error) *error = std::string("方法静态属性不符 ") + ns + "." + cls + "." + method;
        return false;
    }
    *out = mi;
    return true;
}

bool ForceCounts999(const char* property, std::string* error)
{
    std::string getterName = std::string("get_") + property;
    void* getter = Il2Cpp_Method(kGameImage, "GameData.RunTime.Common", "RunTimeStorage", getterName.c_str(), 0);
    void* force = Il2Cpp_Method(kGameImage, "GameData.RunTime.Common", "RunTimeStorage", "ForceObjectsNum", 2);
    if (!getter || !force) {
        if (error) *error = std::string("未找到仓库计数方法 ") + property;
        return false;
    }
    if (!Il2Cpp_MethodIsStatic(getter) || !Il2Cpp_MethodIsStatic(force)) {
        if (error) *error = std::string("仓库计数方法不是静态方法 ") + property;
        return false;
    }

    void* intClass = Il2Cpp_Class("mscorlib.dll", "System", "Int32");
    if (!intClass) intClass = Il2Cpp_Class("System.Private.CoreLib.dll", "System", "Int32");
    if (!intClass) {
        if (error) *error = "未找到 System.Int32";
        return false;
    }
    void* empty = Il2Cpp_ArrayNew(intClass, 0);
    if (!empty) {
        if (error) *error = "创建空数组失败";
        return false;
    }

    void* dictionary = ((void*(*)())Il2Cpp_MethodPointer(getter))();
    if (!dictionary) {
        if (error) *error = std::string("仓库字典为空 ") + property;
        return false;
    }
    ((void(*)(void*, void*))Il2Cpp_MethodPointer(force))(dictionary, empty);
    return true;
}

bool ResolveOnMain(std::string* error)
{
    if (!Il2Cpp_Load(error)) return false;
    Il2Cpp_Attach();

    std::string moneyError;
    bool money = true;
    money = ResolveMethod(kGameImage, "GameData.RunTime.Common", "RunTimePlayerData",
                          "GetFund", 0, true, &g_getFund, &moneyError) && money;
    money = ResolveMethod(kGameImage, "GameData.RunTime.Common", "RunTimePlayerData",
                          "set_Fund", 1, true, &g_setFund, &moneyError) && money;

    std::string speedError;
    bool speed = true;
    speed = ResolveMethod(kUnityImage, "UnityEngine", "Time",
                          "get_timeScale", 0, true, &g_getTimeScale, &speedError) && speed;
    speed = ResolveMethod(kUnityImage, "UnityEngine", "Time",
                          "set_timeScale", 1, true, &g_setTimeScale, &speedError) && speed;

    g_moneyReady = money;
    g_speedReady = speed;
    g_resolved = money || speed;

    std::string featureError;
    if (!g_featuresInitAttempted) {
        bool featuresOk = Features_Init(&featureError);
        g_featuresInitAttempted = true;
        if (!featuresOk && !featureError.empty()) {
            CoreLog("features init failed: %s", featureError.c_str());
        }
    }


    if (!money) {
        if (error) *error = moneyError;
        CoreLog("money resolve failed: %s", moneyError.c_str());
    }
    if (!speed) {
        CoreLog("speed resolve failed: %s", speedError.c_str());
    }
    CoreLog("resolve money=%d speed=%d", money ? 1 : 0, speed ? 1 : 0);
    return money || speed;
}
}

bool Game_GameAssemblyLoaded()
{
    return Il2Cpp_GameAssemblyLoaded();
}

bool Game_IsResolved()
{
    return g_resolved;
}

bool Game_IsMoneyReady()
{
    return g_moneyReady;
}

bool Game_IsSpeedReady()
{
    return g_speedReady;
}

bool Game_EnsureResolved(std::string* error)
{
    if (g_moneyReady && g_speedReady && g_featuresInitAttempted) return true;
    if (!Il2Cpp_GameAssemblyLoaded()) {
        if (error) *error = "GameAssembly.dll 未加载 游戏可能还在启动";
        return false;
    }

    struct State { bool ok; std::string error; State() : ok(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state]() { state->ok = ResolveOnMain(&state->error); }, 5000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->ok) {
        if (error) *error = state->error;
        return false;
    }
    return true;
}

bool Game_GetMoney(long long* out, std::string* error)
{
    if (!out) return false;
    if (!g_moneyReady) {
        if (error) *error = "金钱方法未解析";
        return false;
    }
    void* ptr = Il2Cpp_MethodPointer(g_getFund);
    if (!ptr) {
        if (error) *error = "GetFund 方法指针为空";
        return false;
    }

    struct State { long long value; bool done; State() : value(0), done(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, ptr]() {
            state->value = ((int64_t(*)())ptr)();
            state->done = true;
        }, 3000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->done) {
        if (error) *error = "读取金钱失败";
        return false;
    }
    *out = state->value;
    return true;
}

bool Game_SetMoney(long long value, std::string* error)
{
    if (!g_moneyReady) {
        if (error) *error = "金钱方法未解析";
        return false;
    }
    void* ptr = Il2Cpp_MethodPointer(g_setFund);
    if (!ptr) {
        if (error) *error = "set_Fund 方法指针为空";
        return false;
    }

    struct State { bool done; State() : done(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, ptr, value]() {
            ((void(*)(int64_t))ptr)((int64_t)value);
            state->done = true;
        }, 3000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->done) {
        if (error) *error = "写入金钱失败";
        return false;
    }
    return true;
}

bool Game_GetTimeScale(float* out, std::string* error)
{
    if (!out) return false;
    if (!g_speedReady) {
        if (error) *error = "游戏速度方法未解析";
        return false;
    }
    void* ptr = Il2Cpp_MethodPointer(g_getTimeScale);
    if (!ptr) {
        if (error) *error = "get_timeScale 方法指针为空";
        return false;
    }

    struct State { float value; bool done; State() : value(1.0f), done(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, ptr]() {
            state->value = ((float(*)())ptr)();
            state->done = true;
        }, 3000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->done) {
        if (error) *error = "读取游戏速度失败";
        return false;
    }
    *out = state->value;
    return true;
}

bool Game_SetTimeScale(float value, std::string* error)
{
    if (!g_speedReady) {
        if (error) *error = "游戏速度方法未解析";
        return false;
    }
    void* ptr = Il2Cpp_MethodPointer(g_setTimeScale);
    if (!ptr) {
        if (error) *error = "set_timeScale 方法指针为空";
        return false;
    }

    struct State { bool done; bool ok; std::string error; State() : done(false), ok(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, ptr, value]() {
            state->ok = Features_SetBaseTimeScale(value, &state->error);
            if (!state->ok) {
                ((void(*)(float))ptr)(value);
                state->error.clear();
                state->ok = true;
            }
            state->done = true;
        }, 3000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->done || !state->ok) {
        if (error) *error = state->error.empty() ? "写入游戏速度失败" : state->error;
        return false;
    }
    return true;
}

bool Game_FreezeTime(bool enabled, std::string* error)
{
    if (!g_speedReady) {
        if (error) *error = "时间控制方法未解析";
        return false;
    }
    void* getPtr = Il2Cpp_MethodPointer(g_getTimeScale);
    void* setPtr = Il2Cpp_MethodPointer(g_setTimeScale);
    if (!getPtr || !setPtr) {
        if (error) *error = "时间控制方法指针为空";
        return false;
    }

    if (enabled) {
        if (g_frozen.load()) return true;
        struct State { bool done; float saved; State() : done(false), saved(1.0f) {} };
        std::shared_ptr<State> state = std::make_shared<State>();
        std::string localError;
        if (!MainThread_Run([state, getPtr, setPtr]() {
                state->saved = Features_UnscaleTimeScale(((float(*)())getPtr)());
                ((void(*)(float))setPtr)(0.0f);
                state->done = true;
            }, 3000, &localError)) {
            if (error) *error = localError;
            return false;
        }
        if (!state->done) {
            if (error) *error = "暂停时间失败";
            return false;
        }
        g_savedTimeScale.store(state->saved);
        g_frozen = true;
        return true;
    }

    if (!g_frozen.exchange(false)) return true;
    float saved = g_savedTimeScale.load();
    struct State { bool done; State() : done(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, setPtr, saved]() {
            ((void(*)(float))setPtr)(saved);
            state->done = true;
        }, 3000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->done) {
        if (error) *error = "恢复时间失败";
        return false;
    }
    return true;
}

bool Game_IsFrozen()
{
    return g_frozen.load();
}

bool Game_HoldFreeze()
{
    if (!g_frozen.load() || !g_speedReady) return false;
    void* setPtr = Il2Cpp_MethodPointer(g_setTimeScale);
    if (!setPtr) return false;
    return MainThread_Run([setPtr]() {
        if (!g_frozen.load()) return;
        ((void(*)(float))setPtr)(0.0f);
    }, 2000, NULL);
}

bool Game_Obtain(const std::string& category, std::string* error)
{
    if (!Il2Cpp_GameAssemblyLoaded()) {
        if (error) *error = "GameAssembly.dll 未加载 游戏可能还在启动";
        return false;
    }

    struct DebugDef { const char* key; const char* method; };
    static const DebugDef kDebug[] = {
        {"badges", "GetAllBadges"},
        {"clothes", "GetAllClothes"},
        {"records", "GetAllRecords"},
        {"fishing", "GetAllFishingTrophy"},
        {"partners", "GetAllPartners"},
        {"decorations", "GetAndUseAllDecorations"}
    };

    struct DbDef { const char* key; const char* getAll; const char* inRange; bool hasBool; };
    static const DbDef kDb[] = {
        {"ingredients", "GetAllIngredients", "IngredientInRange", true},
        {"foods", "GetAllFoods", "FoodInRange", true},
        {"beverages", "GetAllBeverages", "BeverageInRange", true},
        {"cookers", "GetAllCookers", "CookerInRange", true},
        {"recipes", "GetAllRecipes", "RecipeInRange", true},
        {"izakayas", "GetAllIzakayas", "IzakayaInRange", false}
    };

    const DebugDef* debugDef = NULL;
    const DbDef* dbDef = NULL;
    for (size_t i = 0; i < sizeof(kDebug) / sizeof(kDebug[0]); i++) {
        if (category == kDebug[i].key) debugDef = &kDebug[i];
    }
    for (size_t i = 0; i < sizeof(kDb) / sizeof(kDb[0]); i++) {
        if (category == kDb[i].key) dbDef = &kDb[i];
    }
    if (!debugDef && !dbDef) {
        if (error) *error = "未知项目: " + category;
        return false;
    }

    std::string methodName = debugDef ? debugDef->method : dbDef->getAll;
    struct State { bool done; bool ok; std::string error; State() : done(false), ok(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;

    if (!MainThread_Run([state, debugDef, dbDef, methodName, category]() {
            if (!Il2Cpp_Load(&state->error)) return;
            Il2Cpp_Attach();
            ObtainGuard obtainGuard;


            if (debugDef) {
                void* mi = Il2Cpp_Method(kGameImage, "PrototypingManagers", "DaySceneDebugImpl", debugDef->method, 0);
                if (!mi) {
                    state->error = std::string("未找到 DaySceneDebugImpl.") + debugDef->method;
                    return;
                }
                if (!Il2Cpp_MethodIsStatic(mi)) {
                    state->error = std::string(debugDef->method) + " 不是静态方法";
                    return;
                }
                void* ptr = Il2Cpp_MethodPointer(mi);
                if (!ptr) {
                    state->error = "方法指针为空: " + methodName;
                    return;
                }
                ((void(*)())ptr)();
                if (category == "badges" && !ForceCounts999("Badges", &state->error)) return;
                if (category == "decorations" && !ForceCounts999("Items", &state->error)) return;
                state->ok = true;
                state->done = true;
                return;
            }

            void* getAll = Il2Cpp_Method(kGameImage, "GameData.Core.Collections", "DataBaseCore", dbDef->getAll, 0);
            void* inRange = Il2Cpp_Method(kGameImage, "GameData.RunTime.Common", "RunTimeStorage", dbDef->inRange, dbDef->hasBool ? 2 : 1);
            if (!getAll || !inRange) {
                state->error = std::string("未找到 ") + dbDef->getAll + " 或 " + dbDef->inRange;
                return;
            }
            if (!Il2Cpp_MethodIsStatic(getAll) || !Il2Cpp_MethodIsStatic(inRange)) {
                state->error = std::string(dbDef->getAll) + " 或 " + dbDef->inRange + " 不是静态方法";
                return;
            }

            void* getAllPtr = Il2Cpp_MethodPointer(getAll);
            void* inRangePtr = Il2Cpp_MethodPointer(inRange);
            if (!getAllPtr || !inRangePtr) {
                state->error = "方法指针为空: " + methodName;
                return;
            }

            GcGuard gc;
            void* ids = ((void*(*)())getAllPtr)();
            if (!ids) {
                state->error = dbDef->getAll + std::string(" 返回空");
                return;
            }
            if (dbDef->hasBool) {
                ((void(*)(void*, bool))inRangePtr)(ids, false);
            } else {
                ((void(*)(void*))inRangePtr)(ids);
            }

            const char* property = NULL;
            if (strcmp(dbDef->key, "ingredients") == 0) property = "Ingredients";
            else if (strcmp(dbDef->key, "foods") == 0) property = "Foods";
            else if (strcmp(dbDef->key, "beverages") == 0) property = "Beverages";
            else if (strcmp(dbDef->key, "cookers") == 0) property = "Cookers";
            if (property && !ForceCounts999(property, &state->error)) return;

            state->ok = true;
            state->done = true;
        }, 8000, &localError)) {
        if (error) *error = localError;
        return false;
    }

    if (!state->done || !state->ok) {
        if (error) *error = state->error.empty() ? "执行失败" : state->error;
        return false;
    }
    return true;
}

bool Game_CheckObtain(const std::string& category, std::string* error)
{
    struct DebugDef { const char* key; const char* method; };
    static const DebugDef kDebug[] = {
        {"badges", "GetAllBadges"},
        {"clothes", "GetAllClothes"},
        {"records", "GetAllRecords"},
        {"fishing", "GetAllFishingTrophy"},
        {"partners", "GetAllPartners"},
        {"decorations", "GetAndUseAllDecorations"}
    };

    struct DbDef { const char* key; const char* getAll; const char* inRange; bool hasBool; };
    static const DbDef kDb[] = {
        {"ingredients", "GetAllIngredients", "IngredientInRange", true},
        {"foods", "GetAllFoods", "FoodInRange", true},
        {"beverages", "GetAllBeverages", "BeverageInRange", true},
        {"cookers", "GetAllCookers", "CookerInRange", true},
        {"recipes", "GetAllRecipes", "RecipeInRange", true},
        {"izakayas", "GetAllIzakayas", "IzakayaInRange", false}
    };

    const DebugDef* debugDef = NULL;
    const DbDef* dbDef = NULL;
    for (size_t i = 0; i < sizeof(kDebug) / sizeof(kDebug[0]); i++) {
        if (category == kDebug[i].key) debugDef = &kDebug[i];
    }
    for (size_t i = 0; i < sizeof(kDb) / sizeof(kDb[0]); i++) {
        if (category == kDb[i].key) dbDef = &kDb[i];
    }
    if (!debugDef && !dbDef) {
        if (error) *error = "未知项目: " + category;
        return false;
    }

    struct State { bool done; bool ok; std::string error; State() : done(false), ok(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, debugDef, dbDef]() {
            if (!Il2Cpp_Load(&state->error)) return;
            Il2Cpp_Attach();

            if (debugDef) {
                void* mi = Il2Cpp_Method(kGameImage, "PrototypingManagers", "DaySceneDebugImpl", debugDef->method, 0);
                if (!mi) {
                    state->error = std::string("未找到 DaySceneDebugImpl.") + debugDef->method;
                    return;
                }
                if (!Il2Cpp_MethodIsStatic(mi)) {
                    state->error = std::string(debugDef->method) + " 不是静态方法";
                    return;
                }
                state->ok = true;
                state->done = true;
                return;
            }

            void* getAll = Il2Cpp_Method(kGameImage, "GameData.Core.Collections", "DataBaseCore", dbDef->getAll, 0);
            void* inRange = Il2Cpp_Method(kGameImage, "GameData.RunTime.Common", "RunTimeStorage", dbDef->inRange, dbDef->hasBool ? 2 : 1);
            if (!getAll || !inRange) {
                state->error = std::string("未找到 ") + dbDef->getAll + " 或 " + dbDef->inRange;
                return;
            }
            if (!Il2Cpp_MethodIsStatic(getAll) || !Il2Cpp_MethodIsStatic(inRange)) {
                state->error = std::string(dbDef->getAll) + " 或 " + dbDef->inRange + " 不是静态方法";
                return;
            }
            state->ok = true;
            state->done = true;
        }, 5000, &localError)) {
        return false;
    }

    if (!state->done || !state->ok) {
        if (error) *error = state->error.empty() ? "检查失败" : state->error;
        return false;
    }
    return true;
}

bool Game_SetFeature(const std::string& key, bool enabled, float value, std::string* error)
{
    if (key == "freeze_time") return Game_FreezeTime(enabled, error);
    if (!Game_EnsureResolved(error)) return false;
    if (!Features_IsKnown(key)) {
        if (error) *error = "未知开关 " + key;
        return false;
    }
    if (!Features_Ready(key)) {
        if (error) *error = "功能未就绪 " + key;
        return false;
    }

    struct State { bool ok; std::string error; State() : ok(false) {} };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::string localError;
    if (!MainThread_Run([state, key, enabled, value]() {
            state->ok = Features_Set(key, enabled, value, &state->error);
        }, 5000, &localError)) {
        if (error) *error = localError;
        return false;
    }
    if (!state->ok) {
        if (error) *error = state->error.empty() ? "设置失败" : state->error;
        return false;
    }
    return true;
}

std::string Game_FeatureReadiness()
{
    return Features_Readiness();
}

std::string Game_FeatureStates()
{
    return Features_States();
}

std::string Game_FeatureValues()
{
    return Features_Values();
}

void Game_DisableFeatures()
{
    if (!g_featuresInitAttempted) return;
    std::string error;
    MainThread_Run([]() { Features_DisableAll(); }, 3000, &error);
}
