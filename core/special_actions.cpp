#include "special_actions.h"
#include "il2cpp_api.h"

#include <cstdio>
#include <cstring>

namespace
{
const char* kGameImage = "Assembly-CSharp.dll";

enum ArgKind
{
    A_NONE,
    A_INT,
    A_BOOL,
    A_STRING,
    A_INT_FIXED,
    A_BOOL_FIXED,
    A_STRING_FIXED
};

struct Def
{
    const char* key;
    const char* ns;
    const char* cls;
    const char* method;
    int argc;
    ArgKind arg;
    int fixedInt;
    const char* fixedText;
};

const Def kActions[] = {
    {"day_register", "PrototypingManagers", "DaySceneDebugImpl", "RegisterMethod", 0, A_NONE, 0, NULL},
    {"day_add_exp", "PrototypingManagers", "DaySceneDebugImpl", "AddExp", 1, A_INT, 0, NULL},
    {"day_max_resources", "PrototypingManagers", "DaySceneDebugImpl", "GetMaxResources", 0, A_NONE, 0, NULL},
    {"day_random_resources", "PrototypingManagers", "DaySceneDebugImpl", "GetRandomResources", 0, A_NONE, 0, NULL},
    {"day_test_resources", "PrototypingManagers", "DaySceneDebugImpl", "GetTestResources", 0, A_NONE, 0, NULL},
    {"day_silver_frog", "PrototypingManagers", "DaySceneDebugImpl", "GetSilverFrogCoin", 1, A_INT, 0, NULL},
    {"day_unlock_special", "PrototypingManagers", "DaySceneDebugImpl", "UnlockSpecialGuestsAndMaps", 0, A_NONE, 0, NULL},
    {"day_unlock_music", "PrototypingManagers", "DaySceneDebugImpl", "UnlockAllMusicChapters", 0, A_NONE, 0, NULL},
    {"day_next_day", "PrototypingManagers", "DaySceneDebugImpl", "NextDay", 0, A_NONE, 0, NULL},
    {"day_set_days", "PrototypingManagers", "DaySceneDebugImpl", "SetDays", 1, A_INT, 0, NULL},
    {"day_add_days", "PrototypingManagers", "DaySceneDebugImpl", "AddDays", 1, A_INT, 0, NULL},
    {"day_warp_hours", "PrototypingManagers", "DaySceneDebugImpl", "WarpHours", 1, A_INT, 0, NULL},
    {"day_warp_actions", "PrototypingManagers", "DaySceneDebugImpl", "WarpActions", 1, A_INT, 0, NULL},
    {"day_clear_ing", "PrototypingManagers", "DaySceneDebugImpl", "ClearAllIngredients", 0, A_NONE, 0, NULL},
    {"day_clear_bev", "PrototypingManagers", "DaySceneDebugImpl", "ClearAllBeverages", 0, A_NONE, 0, NULL},
    {"day_clear_items", "PrototypingManagers", "DaySceneDebugImpl", "ClearAllItems", 0, A_NONE, 0, NULL},
    {"day_start_missions", "PrototypingManagers", "DaySceneDebugImpl", "StartAllMissions", 0, A_NONE, 0, NULL},
    {"day_finish_all", "PrototypingManagers", "DaySceneDebugImpl", "FinishAllEventsAndMissions", 0, A_NONE, 0, NULL},
    {"day_refresh_collectable", "PrototypingManagers", "DaySceneDebugImpl", "RefreshCollectable", 0, A_NONE, 0, NULL},
    {"day_reset_collectable_cd", "PrototypingManagers", "DaySceneDebugImpl", "ResetCollectableCoolDown", 0, A_NONE, 0, NULL},
    {"day_refresh_npc", "PrototypingManagers", "DaySceneDebugImpl", "RefreshNPC", 0, A_NONE, 0, NULL},
    {"day_goto_kourindou", "PrototypingManagers", "DaySceneDebugImpl", "GotoKourindou", 0, A_NONE, 0, NULL},
    {"day_restock_kourindou", "PrototypingManagers", "DaySceneDebugImpl", "AddToKourindoStaticMerchandise", 0, A_NONE, 0, NULL},
    {"day_invite_tewi", "PrototypingManagers", "DaySceneDebugImpl", "InviteProfiteerTewi", 0, A_NONE, 0, NULL},
    {"day_money_box", "PrototypingManagers", "DaySceneDebugImpl", "HakureiMoneyBoxDonate", 1, A_INT, 0, NULL},
    {"day_multi_partner", "PrototypingManagers", "DaySceneDebugImpl", "SetMultiPartnerModeActive", 1, A_BOOL, 0, NULL},
    {"day_max_partner", "PrototypingManagers", "DaySceneDebugImpl", "SetMaxPartnerCount", 1, A_INT, 0, NULL},
    {"day_roguelike", "PrototypingManagers", "DaySceneDebugImpl", "MoveToRogueLike", 0, A_NONE, 0, NULL},
    {"day_reimu_spell", "PrototypingManagers", "DaySceneDebugImpl", "AddReimuPossitiveSpellToWorkScene", 0, A_NONE, 0, NULL},
    {"day_challenge_lv1", "PrototypingManagers", "DaySceneDebugImpl", "StartHakugyokurouChallengeLv1", 0, A_NONE, 0, NULL},
    {"day_challenge_lv2", "PrototypingManagers", "DaySceneDebugImpl", "StartHakugyokurouChallengeLv2", 0, A_NONE, 0, NULL},
    {"day_challenge_final", "PrototypingManagers", "DaySceneDebugImpl", "StartHakugyokurouChallengeFinal", 0, A_NONE, 0, NULL},
    {"day_weird_cooking", "PrototypingManagers", "DaySceneDebugImpl", "StartWeirdCookingChallenge", 0, A_NONE, 0, NULL},
    {"day_festival_music", "PrototypingManagers", "DaySceneDebugImpl", "StartHakureiFestivalMusicChallenge", 0, A_NONE, 0, NULL},
    {"day_festival_cooking", "PrototypingManagers", "DaySceneDebugImpl", "StartHakureiFestivalCookingChallenge", 0, A_NONE, 0, NULL},
    {"day_creators_box", "PrototypingManagers", "DaySceneDebugImpl", "OpenCreatorsBoxPanel", 0, A_NONE, 0, NULL},
    {"day_panel1", "PrototypingManagers", "DaySceneDebugImpl", "OpenInfiniteSelectorPanel1", 0, A_NONE, 0, NULL},
    {"day_panel2", "PrototypingManagers", "DaySceneDebugImpl", "OpenInfiniteSelectorPanel2", 0, A_NONE, 0, NULL},
    {"day_panel3", "PrototypingManagers", "DaySceneDebugImpl", "OpenInfiniteSelectorPanel3", 0, A_NONE, 0, NULL},
    {"day_collider", "PrototypingManagers", "DaySceneDebugImpl", "SetPlayerColliderActive", 1, A_BOOL, 0, NULL},
    {"day_input", "PrototypingManagers", "DaySceneDebugImpl", "SetPlayerInputActive", 1, A_BOOL, 0, NULL},

    {"map_beast_forest", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "BeastForest"},
    {"map_human_village", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "HumanVillage"},
    {"map_hakurei_shrine", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "HakureiShrine"},
    {"map_scarlet_mansion", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "ScarletMansion"},
    {"map_bamboo_forest", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "BambooForest"},
    {"map_hakugyokurou", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "Hakugyokurou"},
    {"map_youkai_mountain", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "YoukaiMountain"},
    {"map_magic_forest", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "MagicForest"},
    {"map_former_hell", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "FormerHell"},
    {"map_myouren_temple", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "MyourenTemple"},
    {"map_garden_of_the_sun", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "GardenOfTheSun"},
    {"map_flandre_home", "PrototypingManagers", "DaySceneDebugImpl", "Transmit", 1, A_STRING_FIXED, 0, "FlandreHome"},

    {"night_register", "PrototypingManagers", "NightSceneDebugImpl", "RegisterMethod", 0, A_NONE, 0, NULL},
    {"night_cheated_buff", "PrototypingManagers", "NightSceneDebugImpl", "RegisteredCheatedBuff", 0, A_NONE, 0, NULL},
    {"night_eternity_fever", "PrototypingManagers", "NightSceneDebugImpl", "RegisteredEternityFever", 0, A_NONE, 0, NULL},
    {"night_remove_buffs", "PrototypingManagers", "NightSceneDebugImpl", "RemoveAllRegisteredTimedBuff", 0, A_NONE, 0, NULL},
    {"night_free_cook", "PrototypingManagers", "NightSceneDebugImpl", "SetFreeCook", 1, A_INT, 0, NULL},
    {"night_income_add", "PrototypingManagers", "NightSceneDebugImpl", "SetWorkIncome", 2, A_INT_FIXED, 0, NULL},
    {"night_income_set", "PrototypingManagers", "NightSceneDebugImpl", "SetWorkIncome", 2, A_INT_FIXED, 1, NULL},
    {"night_combo_add", "PrototypingManagers", "NightSceneDebugImpl", "SetWorkCombo", 2, A_INT_FIXED, 0, NULL},
    {"night_combo_set", "PrototypingManagers", "NightSceneDebugImpl", "SetWorkCombo", 2, A_INT_FIXED, 1, NULL},
    {"night_modify_time", "PrototypingManagers", "NightSceneDebugImpl", "ModifyWorkTime", 1, A_INT, 0, NULL},
    {"night_refill_bev", "PrototypingManagers", "NightSceneDebugImpl", "SupplementaryBeverages", 0, A_NONE, 0, NULL},
    {"night_refill_ing", "PrototypingManagers", "NightSceneDebugImpl", "SupplementaryIngredients", 0, A_NONE, 0, NULL},
    {"night_refill_food", "PrototypingManagers", "NightSceneDebugImpl", "SupplementaryFoods", 0, A_NONE, 0, NULL},
    {"night_spawn_normal", "PrototypingManagers", "NightSceneDebugImpl", "SpawnNormalGuest", 0, A_NONE, 0, NULL},
    {"night_spawn_special", "PrototypingManagers", "NightSceneDebugImpl", "SpawnSpecialGuest", 0, A_NONE, 0, NULL},
    {"night_pay_leave", "PrototypingManagers", "NightSceneDebugImpl", "RepellGuestAndPay", 0, A_NONE, 0, NULL},
    {"night_qte_random", "PrototypingManagers", "NightSceneDebugImpl", "TriggerRandomQTEBuff", 1, A_BOOL_FIXED, 1, NULL},
    {"night_qte_throw", "PrototypingManagers", "NightSceneDebugImpl", "TriggerThrowServingQTEBuff", 0, A_NONE, 0, NULL},
    {"night_qte_patient", "PrototypingManagers", "NightSceneDebugImpl", "TriggerPatientQTEBuff", 0, A_NONE, 0, NULL},
    {"night_qte_finish", "PrototypingManagers", "NightSceneDebugImpl", "TriggerFinishEatingImmediatelyQTEBuff", 0, A_NONE, 0, NULL},
    {"night_passion_up", "PrototypingManagers", "NightSceneDebugImpl", "IncreasePassionLevel", 0, A_NONE, 0, NULL},
    {"night_passion_down", "PrototypingManagers", "NightSceneDebugImpl", "DecreasePassionLevel", 0, A_NONE, 0, NULL},
    {"night_throw_deliver", "PrototypingManagers", "NightSceneDebugImpl", "ThrowDeliver", 1, A_INT, 0, NULL},
    {"night_fever", "PrototypingManagers", "NightSceneDebugImpl", "Fever", 1, A_INT, 0, NULL},

    {"global_console_on", "PrototypingManagers", "GlobalDebugConsole", "set_EnableConsole", 1, A_BOOL_FIXED, 1, NULL},
    {"global_console_off", "PrototypingManagers", "GlobalDebugConsole", "set_EnableConsole", 1, A_BOOL_FIXED, 0, NULL},
    {"global_goto_main", "PrototypingManagers", "GlobalDebugImpl", "GotoMainScene", 0, A_NONE, 0, NULL},
    {"global_goto_empty", "PrototypingManagers", "GlobalDebugImpl", "GotoEmptyScene", 0, A_NONE, 0, NULL},
    {"global_export", "PrototypingManagers", "GlobalDebugImpl", "OutputArchiveAndLog", 0, A_NONE, 0, NULL},
    {"global_open_log", "PrototypingManagers", "GlobalDebugImpl", "OpenLogDirectory", 0, A_NONE, 0, NULL},
    {"main_test_result", "PrototypingManagers", "MainSceneDebugImpl", "GotoTestResultScene", 0, A_NONE, 0, NULL},

    {"ach_check_all", "GameData.RunTime.Common.AchievementSystem", "DataBaseAchievement", "CheckAllAchievement", 0, A_NONE, 0, NULL},
    {"ach_finish", "GameData.RunTime.Common.AchievementSystem", "DataBaseAchievement", "FinishAchievement", 1, A_STRING, 0, NULL},
    {"ach_finish_check", "GameData.RunTime.Common.AchievementSystem", "DataBaseAchievement", "FinishAndCheckAchievement", 1, A_STRING, 0, NULL},
};

const Def* Find(const std::string& key)
{
    for (size_t i = 0; i < sizeof(kActions) / sizeof(kActions[0]); i++) {
        if (key == kActions[i].key) return &kActions[i];
    }
    return NULL;
}
}

bool Special_Call(const std::string& key, int value, int value2, const std::string& text,
                  std::string* error)
{
    const Def* def = Find(key);
    if (!def) {
        if (error) *error = "未知特殊功能 " + key;
        return false;
    }
    void* mi = Il2Cpp_Method(kGameImage, def->ns, def->cls, def->method, def->argc);
    if (!mi) {
        if (error) *error = std::string("未找到 ") + def->cls + "." + def->method;
        return false;
    }
    if (!Il2Cpp_MethodIsStatic(mi)) {
        if (error) *error = std::string(def->cls) + "." + def->method + " 不是静态方法";
        return false;
    }
    void* fn = Il2Cpp_MethodPointer(mi);
    if (!fn) {
        if (error) *error = std::string("方法指针为空 ") + def->cls + "." + def->method;
        return false;
    }

    void* str = NULL;
    if (def->arg == A_STRING || def->arg == A_STRING_FIXED) {
        std::string s = def->arg == A_STRING_FIXED ? def->fixedText : text;
        str = Il2Cpp_StringNew(s.c_str());
        if (!str) {
            if (error) *error = "创建字符串失败";
            return false;
        }
    }

    switch (def->arg) {
        case A_NONE:
            ((void(*)())fn)();
            break;
        case A_INT:
            ((void(*)(int))fn)(value);
            break;
        case A_BOOL:
            ((void(*)(bool))fn)(value != 0);
            break;
        case A_STRING:
        case A_STRING_FIXED:
            ((void(*)(void*))fn)(str);
            break;
        case A_INT_FIXED:
            ((void(*)(int, int))fn)(value, def->fixedInt);
            break;
        case A_BOOL_FIXED:
            ((void(*)(bool))fn)(def->fixedInt != 0);
            break;
        default:
            if (error) *error = "未知参数类型";
            return false;
    }
    (void)value2;
    return true;
}
