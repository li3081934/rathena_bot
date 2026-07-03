#include "bot_ctrl.hpp"

#include <algorithm>
#include <set>
#include <thread>
#include <unordered_map>

#include <common/malloc.hpp>
#include <common/nullpo.hpp>
#include <common/showmsg.hpp>
#include <common/sql.hpp>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include <common/mapindex.hpp>
#include "map.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "itemdb.hpp"
#include "skill.hpp"
#include "status.hpp"
#include "unit.hpp"

static std::mutex bot_ctrl_db_mutex;
static std::unordered_map<int32, bot_ctrl*> bot_ctrl_db;

extern Sql* mmysql_handle;
static const char* bot_rules_table = "bot_rules";

/***********************************************************************
 *  查找
 ***********************************************************************/
bot_ctrl* bot_ctrl_search(map_session_data* master) {
    if (!master) return nullptr;
    std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
    auto it = bot_ctrl_db.find(master->status.char_id);
    return (it != bot_ctrl_db.end()) ? it->second : nullptr;
}

/***********************************************************************
 *  从 DB 加载角色数据到 mmo_charstatus
 ***********************************************************************/
static bool load_char_status(int32 char_id, struct mmo_charstatus* st) {
    char* data;
    memset(st, 0, sizeof(*st));
    if (SQL_ERROR == Sql_Query(mmysql_handle,
        "SELECT `char_id`,`account_id`,`name`,`class`,`base_level`,`job_level`,"
        "`base_exp`,`job_exp`,`zeny`,`str`,`agi`,`vit`,`int`,`dex`,`luk`,"
        "`max_hp`,`hp`,`max_sp`,`sp`,`status_point`,`skill_point`,"
        "`hair`,`hair_color`,`clothes_color`,`body`,"
        "`weapon`,`shield`,`head_top`,`head_mid`,`head_bottom`,`robe`,"
        "`last_map`,`last_x`,`last_y`,`save_map`,`save_x`,`save_y`,"
        "`party_id`,`guild_id`,`pet_id`,`homun_id`,`elemental_id`,"
        "`inventory_slots`,`sex` FROM `char` WHERE `char_id` = %d", char_id))
        return false;
    if (Sql_NumRows(mmysql_handle) == 0 || SQL_ERROR == Sql_NextRow(mmysql_handle)) {
        Sql_FreeResult(mmysql_handle);
        return false;
    }

    Sql_GetData(mmysql_handle, 0, &data, nullptr); st->char_id = atoi(data);
    Sql_GetData(mmysql_handle, 1, &data, nullptr); st->account_id = atoi(data);
    Sql_GetData(mmysql_handle, 2, &data, nullptr); safestrncpy(st->name, data, NAME_LENGTH);
    Sql_GetData(mmysql_handle, 3, &data, nullptr); st->class_ = atoi(data);
    Sql_GetData(mmysql_handle, 4, &data, nullptr); st->base_level = atoi(data);
    Sql_GetData(mmysql_handle, 5, &data, nullptr); st->job_level = atoi(data);
    Sql_GetData(mmysql_handle, 6, &data, nullptr); st->base_exp = atoll(data);
    Sql_GetData(mmysql_handle, 7, &data, nullptr); st->job_exp = atoll(data);
    Sql_GetData(mmysql_handle, 8, &data, nullptr); st->zeny = atoi(data);
    Sql_GetData(mmysql_handle, 9, &data, nullptr); st->str = atoi(data);
    Sql_GetData(mmysql_handle, 10, &data, nullptr); st->agi = atoi(data);
    Sql_GetData(mmysql_handle, 11, &data, nullptr); st->vit = atoi(data);
    Sql_GetData(mmysql_handle, 12, &data, nullptr); st->int_ = atoi(data);
    Sql_GetData(mmysql_handle, 13, &data, nullptr); st->dex = atoi(data);
    Sql_GetData(mmysql_handle, 14, &data, nullptr); st->luk = atoi(data);
    Sql_GetData(mmysql_handle, 15, &data, nullptr); st->max_hp = atoi(data);
    Sql_GetData(mmysql_handle, 16, &data, nullptr); st->hp = atoi(data);
    Sql_GetData(mmysql_handle, 17, &data, nullptr); st->max_sp = atoi(data);
    Sql_GetData(mmysql_handle, 18, &data, nullptr); st->sp = atoi(data);
    Sql_GetData(mmysql_handle, 19, &data, nullptr); st->status_point = atoi(data);
    Sql_GetData(mmysql_handle, 20, &data, nullptr); st->skill_point = atoi(data);
    // hair, hair_color, clothes_color, body
    Sql_GetData(mmysql_handle, 21, &data, nullptr); st->hair = atoi(data);
    Sql_GetData(mmysql_handle, 22, &data, nullptr); st->hair_color = atoi(data);
    Sql_GetData(mmysql_handle, 23, &data, nullptr); st->clothes_color = atoi(data);
    Sql_GetData(mmysql_handle, 24, &data, nullptr); st->body = atoi(data);
    // weapon, shield, head_top/mid/bottom, robe
    Sql_GetData(mmysql_handle, 25, &data, nullptr); st->weapon = atoi(data);
    Sql_GetData(mmysql_handle, 26, &data, nullptr); st->shield = atoi(data);
    Sql_GetData(mmysql_handle, 27, &data, nullptr); st->head_top = atoi(data);
    Sql_GetData(mmysql_handle, 28, &data, nullptr); st->head_mid = atoi(data);
    Sql_GetData(mmysql_handle, 29, &data, nullptr); st->head_bottom = atoi(data);
    Sql_GetData(mmysql_handle, 30, &data, nullptr); st->robe = atoi(data);
    // last_map/x/y, save_map/x/y
    Sql_GetData(mmysql_handle, 31, &data, nullptr); safestrncpy(st->last_point.map, data, MAP_NAME_LENGTH_EXT);
    Sql_GetData(mmysql_handle, 32, &data, nullptr); st->last_point.x = atoi(data);
    Sql_GetData(mmysql_handle, 33, &data, nullptr); st->last_point.y = atoi(data);
    Sql_GetData(mmysql_handle, 34, &data, nullptr); safestrncpy(st->save_point.map, data, MAP_NAME_LENGTH_EXT);
    Sql_GetData(mmysql_handle, 35, &data, nullptr); st->save_point.x = atoi(data);
    Sql_GetData(mmysql_handle, 36, &data, nullptr); st->save_point.y = atoi(data);
    // party, guild, pet, homun, elemental ids
    Sql_GetData(mmysql_handle, 37, &data, nullptr); st->party_id = atoi(data);
    Sql_GetData(mmysql_handle, 38, &data, nullptr); st->guild_id = atoi(data);
    Sql_GetData(mmysql_handle, 39, &data, nullptr); st->pet_id = atoi(data);
    Sql_GetData(mmysql_handle, 40, &data, nullptr); st->hom_id = atoi(data);
    Sql_GetData(mmysql_handle, 41, &data, nullptr); st->ele_id = atoi(data);
    // inventory_slots
    Sql_GetData(mmysql_handle, 42, &data, nullptr); st->inventory_slots = (uint16)atoi(data);
    // sex
    Sql_GetData(mmysql_handle, 43, &data, nullptr); st->sex = (data[0] == 'F') ? SEX_FEMALE : SEX_MALE;

    Sql_FreeResult(mmysql_handle);
    return true;
}

/***********************************************************************
 *  从 DB 加载背包
 ***********************************************************************/
static bool load_bot_inventory(map_session_data* sd, int32 char_id) {
    memset(&sd->inventory, 0, sizeof(sd->inventory));
    sd->inventory.amount = 0;
    sd->inventory.id = char_id;
    sd->inventory.type = TABLE_INVENTORY;

    if (SQL_ERROR == Sql_Query(mmysql_handle,
        "SELECT `id`,`nameid`,`amount`,`equip`,`identify`,`refine`,`attribute`,"
        "`card0`,`card1`,`card2`,`card3`,"
        "`option_id0`,`option_val0`,`option_parm0`,"
        "`option_id1`,`option_val1`,`option_parm1`,"
        "`option_id2`,`option_val2`,`option_parm2`,"
        "`option_id3`,`option_val3`,`option_parm3`,"
        "`option_id4`,`option_val4`,`option_parm4`,"
        "`expire_time`,`favorite`,`bound`,`unique_id`,`equip_switch`,`enchantgrade` "
        "FROM `inventory` WHERE `char_id` = %d ORDER BY `id`", char_id))
        return false;

    int32 idx = 0;
    char* data;

    while (SQL_SUCCESS == Sql_NextRow(mmysql_handle) && idx < MAX_INVENTORY) {
        struct item* it = &sd->inventory.u.items_inventory[idx];
        int col = 0;
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->id = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->nameid = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->amount = (int16)atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->equip = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->identify = (char)atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->refine = (char)atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->attribute = (char)atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->card[0] = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->card[1] = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->card[2] = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->card[3] = atoi(data);
        for (int o = 0; o < MAX_ITEM_RDM_OPT; o++) {
            Sql_GetData(mmysql_handle, col++, &data, nullptr); it->option[o].id = (int16)atoi(data);
            Sql_GetData(mmysql_handle, col++, &data, nullptr); it->option[o].value = (int16)atoi(data);
            Sql_GetData(mmysql_handle, col++, &data, nullptr); it->option[o].param = (char)atoi(data);
        }
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->expire_time = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->favorite = (char)atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->bound = (char)atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->unique_id = strtoull(data, nullptr, 10);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->equipSwitch = atoi(data);
        Sql_GetData(mmysql_handle, col++, &data, nullptr); it->enchantgrade = (uint8)atoi(data);

        sd->inventory_data[idx] = item_db.find(it->nameid).get();
        idx++;
    }
    Sql_FreeResult(mmysql_handle);
    sd->inventory.amount = idx;
    return true;
}

/***********************************************************************
 *  保存背包到 DB
 ***********************************************************************/
static bool save_bot_inventory(map_session_data* sd) {
    Sql_Query(mmysql_handle, "DELETE FROM `inventory` WHERE `char_id` = %d",
              sd->status.char_id);

    for (int32 i = 0; i < MAX_INVENTORY; i++) {
        struct item* it = &sd->inventory.u.items_inventory[i];
        if (it->nameid == 0) continue;

        if (SQL_ERROR == Sql_Query(mmysql_handle,
            "INSERT INTO `inventory` "
            "(`char_id`,`nameid`,`amount`,`equip`,`identify`,`refine`,`attribute`,"
            "`card0`,`card1`,`card2`,`card3`,"
            "`option_id0`,`option_val0`,`option_parm0`,"
            "`option_id1`,`option_val1`,`option_parm1`,"
            "`option_id2`,`option_val2`,`option_parm2`,"
            "`option_id3`,`option_val3`,`option_parm3`,"
            "`option_id4`,`option_val4`,`option_parm4`,"
            "`expire_time`,`favorite`,`bound`,`unique_id`,`equip_switch`,`enchantgrade`) "
            "VALUES (%d,%u,%d,%u,%d,%d,%d,"
            "%u,%u,%u,%u,"
            "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
            "%u,%d,%d,%llu,%u,%d)",
            sd->status.char_id, it->nameid, it->amount, it->equip,
            it->identify, it->refine, it->attribute,
            it->card[0], it->card[1], it->card[2], it->card[3],
            it->option[0].id, it->option[0].value, it->option[0].param,
            it->option[1].id, it->option[1].value, it->option[1].param,
            it->option[2].id, it->option[2].value, it->option[2].param,
            it->option[3].id, it->option[3].value, it->option[3].param,
            it->option[4].id, it->option[4].value, it->option[4].param,
            it->expire_time, it->favorite, it->bound,
            (uint64)it->unique_id, it->equipSwitch, it->enchantgrade))
        {
            ShowError("bot_ctrl: failed to save inventory item %d for char %d: %s\n",
                      it->nameid, sd->status.char_id, Sql_GetError(mmysql_handle));
        }
    }
    return true;
}

/***********************************************************************
 *  AI - Rule Engine
 ***********************************************************************/

struct bot_mob_search {
    block_list* best;
    int32 dist;
    block_list* origin;
};

static int32 bot_find_mob_sub(block_list* bl, va_list ap) {
    bot_mob_search* search = va_arg(ap, bot_mob_search*);
    if (bl == search->origin) return 0;
    if (bl->type != BL_MOB) return 0;
    if (!status_check_skilluse(search->origin, bl, 0, 0)) return 0;
    if (battle_check_target(search->origin, bl, BCT_ENEMY) <= 0) return 0;
    int32 d = distance_bl(search->origin, bl);
    if (!search->best || d < search->dist) {
        search->best = bl;
        search->dist = d;
    }
    return 0;
}

static block_list* bot_find_nearest_mob(block_list* origin, int16 range) {
    bot_mob_search search = { nullptr, 9999, origin };
    map_foreachinrange(bot_find_mob_sub, origin, range, BL_MOB, &search);
    return search.best;
}

static void bot_follow_master(map_session_data* bot, map_session_data* sd) {
    if (!bot || !sd) return;
    if (check_distance_bl(sd, bot, 3)) return;
    if (DIFF_TICK(gettick(), bot->ud.canmove_tick) < 0) return;
    unit_walktobl(bot, sd, 2, 0);
}

// Evaluate a single condition
static bool bot_eval_cond(bot_ctrl* ctrl, BotRuleCond& c) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return false;
    map_session_data* t = (c.source == SRC_MASTER) ? sd : bot;

    switch (c.type) {
    case COND_HP_PCT: {
        int32 actual = 0;
        if (t->battle_status.max_hp > 0)
            actual = (int32)((int64)t->battle_status.hp * 100 / t->battle_status.max_hp);
        switch (c.op) {
        case OP_LT: return actual < c.value;
        case OP_GT: return actual > c.value;
        case OP_EQ: return actual == c.value;
        case OP_NE: return actual != c.value;
        case OP_LE: return actual <= c.value;
        case OP_GE: return actual >= c.value;
        }
        return false;
    }
    case COND_SP_PCT: {
        int32 actual = 0;
        if (t->battle_status.max_sp > 0)
            actual = (int32)((int64)t->battle_status.sp * 100 / t->battle_status.max_sp);
        switch (c.op) {
        case OP_LT: return actual < c.value;
        case OP_GT: return actual > c.value;
        case OP_EQ: return actual == c.value;
        case OP_NE: return actual != c.value;
        case OP_LE: return actual <= c.value;
        case OP_GE: return actual >= c.value;
        }
        return false;
    }
    case COND_SC_MISSING:
        return t->sc.getSCE((sc_type)c.extra) == nullptr;
    case COND_SC_ACTIVE:
        return t->sc.getSCE((sc_type)c.extra) != nullptr;
    case COND_IS_DEAD:
        return t->status.hp <= 0;
    case COND_DIST_GT:
        return !check_distance_bl(sd, bot, c.value);
    case COND_DIST_LT:
        return check_distance_bl(sd, bot, c.value);
    case COND_HAS_ENEMY:
        return bot_find_nearest_mob(bot, (int16)c.value) != nullptr;
    }
    return false;
}

// Execute a single action
static void bot_exec_action(bot_ctrl* ctrl, BotRuleAction& a) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return;

    switch (a.type) {
    case ACT_USE_SKILL: {
        block_list* target;
        if (a.target == 0)
            target = sd;
        else
            target = bot;
        uint16 lv = pc_checkskill(bot, a.skill_id);
        if (lv > 0 && bot->battle_status.sp >= skill_get_sp(a.skill_id, lv))
            unit_skilluse_id(bot, target->id, a.skill_id, lv);
        break;
    }
    case ACT_ATTACK_NEAREST: {
        block_list* mob = bot_find_nearest_mob(bot, 8);
        if (mob) {
            unit_attack(bot, mob->id, 1);
            ctrl->target_mob_id = mob->id;
        }
        break;
    }
    case ACT_RECALL: {
        if (bot->prev != nullptr)
            unit_remove_map(bot, CLR_OUTSIGHT);
        bot->mapindex = sd->mapindex;
        bot->m = sd->m;
        bot->x = sd->x + 1;
        bot->y = sd->y;
        map_addblock(bot);
        clif_spawn(bot);
        break;
    }
    case ACT_SAY: {
        if (a.message[0])
            clif_displaymessage(sd->fd, a.message);
        break;
    }
    }
}

// Evaluate all rules - returns true if any rule fired
static bool bot_eval_rules(bot_ctrl* ctrl) {
    for (auto& rule : ctrl->rules) {
        if (!rule.enabled) continue;

        bool match;
        if (rule.cond_logic == COND_AND) {
            match = true;
            for (auto& c : rule.conditions)
                if (!bot_eval_cond(ctrl, c)) { match = false; break; }
        } else {
            match = false;
            for (auto& c : rule.conditions)
                if (bot_eval_cond(ctrl, c)) { match = true; break; }
        }
        if (!match) continue;

        // Fire this rule: execute all actions
        for (auto& a : rule.actions)
            bot_exec_action(ctrl, a);
        return true;
    }
    return false;
}

// Main AI subroutine
static int32 bot_ctrl_ai_sub(bot_ctrl* ctrl, t_tick tick) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd || !bot->state.active || bot->m != sd->m)
        return 0;
    if (DIFF_TICK(tick, ctrl->last_thinktime) < MIN_BOT_THINKTIME)
        return 0;
    ctrl->last_thinktime = tick;
    if (bot->ud.skilltimer != INVALID_TIMER)
        return 0;

    // Standby mode: skip rules entirely, just stand idle
    if (ctrl->ai_mode == AI_STANDBY)
        return 0;

    // Try rules first
    if (bot_eval_rules(ctrl))
        return 0;

    // No rule fired → fallback to mode default
    if (ctrl->ai_mode == AI_FOLLOW)
        bot_follow_master(bot, sd);
    return 0;
}

static int32 bot_ctrl_ai_foreach(map_session_data* sd, va_list ap) {
    t_tick tick = va_arg(ap, t_tick);
    bot_ctrl* ctrl = nullptr;
    {
        std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
        auto it = bot_ctrl_db.find(sd->status.char_id);
        if (it != bot_ctrl_db.end())
            ctrl = it->second;
    }
    if (ctrl)
        bot_ctrl_ai_sub(ctrl, tick);
    return 0;
}

static TIMER_FUNC(bot_ctrl_ai_timer) {
    map_foreachpc(bot_ctrl_ai_foreach, tick);
    return 0;
}

/***********************************************************************
 *  Job group mapping
 ***********************************************************************/

bot_job_group bot_class_to_group(int32 class_) {
    int32 base = class_ & MAPID_FIRSTMASK;
    switch (base) {
    case MAPID_NOVICE:   return JOBGROUP_NOVICE;
    case MAPID_SWORDMAN: return JOBGROUP_SWORDMAN;
    case MAPID_MAGE:     return JOBGROUP_MAGE;
    case MAPID_ARCHER:   return JOBGROUP_ARCHER;
    case MAPID_ACOLYTE:  return JOBGROUP_ACOLYTE;
    case MAPID_MERCHANT: return JOBGROUP_MERCHANT;
    case MAPID_THIEF:    return JOBGROUP_THIEF;
    default:             return JOBGROUP_NOVICE;
    }
}

/***********************************************************************
 *  Rules DB - load/save
 ***********************************************************************/

static bool bot_ctrl_load_rules(bot_ctrl* ctrl) {
    ctrl->rules.clear();
    char* data;
    if (SQL_ERROR == Sql_Query(mmysql_handle,
        "SELECT rules_json FROM %s WHERE master_account_id = %d",
        bot_rules_table, ctrl->master_aid))
        return false;

    if (Sql_NumRows(mmysql_handle) > 0 && SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
        Sql_GetData(mmysql_handle, 0, &data, nullptr);
        Sql_FreeResult(mmysql_handle);
        if (!data || !data[0]) return true;

        try {
            auto j = nlohmann::json::parse(data);
            for (auto& jr : j) {
                BotRule r;
                std::string n = jr.value("name", "");
                strncpy(r.name, n.c_str(), RULE_NAME_LEN - 1);
                r.name[RULE_NAME_LEN - 1] = '\0';
                r.enabled = jr.value("enabled", true);
                r.cond_logic = jr.value("cond_logic", 0);

                for (auto& jc : jr.value("conditions", nlohmann::json::array())) {
                    BotRuleCond c;
                    c.source = jc.value("source", 0);
                    c.type   = jc.value("type", 0);
                    c.op     = jc.value("op", 0);
                    c.value  = jc.value("value", 0);
                    c.extra  = jc.value("extra", 0);
                    r.conditions.push_back(c);
                }
                for (auto& ja : jr.value("actions", nlohmann::json::array())) {
                    BotRuleAction a;
                    memset(&a, 0, sizeof(a));
                    a.type     = ja.value("type", 0);
                    a.target   = ja.value("target", 0);
                    a.skill_id = ja.value("skill_id", 0);
                    std::string msg = ja.value("message", "");
                    strncpy(a.message, msg.c_str(), 63);
                    a.message[63] = '\0';
                    r.actions.push_back(a);
                }
                ctrl->rules.push_back(r);
            }
        } catch (const std::exception& e) {
            ShowWarning("bot_ctrl: failed to parse rules for AID %d: %s\n", ctrl->master_aid, e.what());
        }
    } else {
        Sql_FreeResult(mmysql_handle);
    }
    return true;
}

static bool bot_ctrl_save_rules(bot_ctrl* ctrl) {
    nlohmann::json j = nlohmann::json::array();
    for (auto& r : ctrl->rules) {
        nlohmann::json jr;
        jr["name"] = r.name;
        jr["enabled"] = r.enabled;
        jr["cond_logic"] = r.cond_logic;

        nlohmann::json jconds = nlohmann::json::array();
        for (auto& c : r.conditions) {
            nlohmann::json jc;
            jc["source"] = c.source;
            jc["type"]   = c.type;
            jc["op"]     = c.op;
            jc["value"]  = c.value;
            jc["extra"]  = c.extra;
            jconds.push_back(jc);
        }
        jr["conditions"] = jconds;

        nlohmann::json jacts = nlohmann::json::array();
        for (auto& a : r.actions) {
            nlohmann::json ja;
            ja["type"]     = a.type;
            ja["target"]   = a.target;
            ja["skill_id"] = a.skill_id;
            ja["message"]  = a.message;
            jacts.push_back(ja);
        }
        jr["actions"] = jacts;
        j.push_back(jr);
    }

    std::string json_str = j.dump();
    size_t esc_len = json_str.size() * 2 + 1;
    char* esc = (char*)aMalloc(esc_len);
    Sql_EscapeStringLen(mmysql_handle, esc, json_str.data(), (int)json_str.size());

    int rc = Sql_Query(mmysql_handle,
        "REPLACE INTO %s (master_account_id, rules_json) VALUES (%d, '%s')",
        bot_rules_table, ctrl->master_aid, esc);

    aFree(esc);
    return rc != SQL_ERROR;
}

/***********************************************************************
 *  内部：将 Bot 角色带上线的公共逻辑
 ***********************************************************************/
static bot_ctrl* bot_ctrl_bring_online(map_session_data* master, int32 bot_aid, int32 bot_cid) {
    // 1. 加载角色数据
    struct mmo_charstatus st;
    if (!load_char_status(bot_cid, &st))
        return nullptr;

    // 2. 创建 map_session_data, fd=0
    map_session_data* bot_sd = nullptr;
    CREATE(bot_sd, TBL_PC, 1);
    new (bot_sd) map_session_data();
    pc_setnewpc(bot_sd, bot_aid, bot_cid, 0, gettick(), master->status.sex, 0);
    bot_sd->fd = 0;
    safestrncpy(bot_sd->status.name, st.name, NAME_LENGTH);

    // 3. 设出生位置为主人所在位置
    safestrncpy(st.last_point.map, mapindex_id2name(master->mapindex), MAP_NAME_LENGTH_EXT);
    st.last_point.x = master->x;
    st.last_point.y = master->y;

    // 4. 直接调用 pc_authok
    if (!pc_authok(bot_sd, 0, 0, 0, &st, false)) {
        aFree(bot_sd);
        return nullptr;
    }

    // 修正 body（db 可能为 0）
    if (!job_db.exists(bot_sd->status.body))
        bot_sd->status.body = bot_sd->status.class_;

    // 5. 手动完成上线（没有客户端发 LoadEndAck）
    if (!bot_sd->state.active) {
        pc_reg_received(bot_sd);
    } else {
        // state.active 已是 1，但还是要走 map_addiddb
        map_addiddb(bot_sd);
    }

    // 确保 Bot 有 HP（db 可能保存了 0）
    if (bot_sd->status.hp <= 0)
        bot_sd->status.hp = bot_sd->status.max_hp;

    // 6. 从 skill 表加载已学技能（pc_authok 不会自动加载）
    if (SQL_SUCCESS == Sql_Query(mmysql_handle,
        "SELECT `id`, `lv`, `flag` FROM `skill` WHERE `char_id` = %d", bot_cid)
    ) {
        while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
            char* data;
            uint16 skill_id;
            uint8 skill_lv;
            uint8 skill_flag;
            Sql_GetData(mmysql_handle, 0, &data, nullptr); skill_id = (uint16)atoi(data);
            Sql_GetData(mmysql_handle, 1, &data, nullptr); skill_lv = (uint8)atoi(data);
            Sql_GetData(mmysql_handle, 2, &data, nullptr); skill_flag = (uint8)atoi(data);
            int32 idx = skill_get_index(skill_id);
            if (idx > 0) {
                bot_sd->status.skill[idx].id = skill_id;
                bot_sd->status.skill[idx].lv = skill_lv;
                bot_sd->status.skill[idx].flag = skill_flag;
            }
        }
        Sql_FreeResult(mmysql_handle);
    }

    // 7. 从 DB 加载背包
    load_bot_inventory(bot_sd, bot_cid);
    // 根据已加载的背包重建装备索引（equip_index[]）
    pc_setequipindex(bot_sd);

    status_set_viewdata(bot_sd, bot_sd->status.class_);
    map_addblock(bot_sd);
    clif_spawn(bot_sd);

    // 5. 控制器
    bot_ctrl* ctrl = (bot_ctrl*)aCalloc(1, sizeof(bot_ctrl));
    ctrl->bot_sd = bot_sd;
    ctrl->master_sd = master;
    ctrl->master_cid = master->status.char_id;
    ctrl->master_aid = master->status.account_id;
    ctrl->bot_aid = bot_aid;
    ctrl->bot_cid = bot_cid;
    ctrl->ai_mode = AI_FOLLOW;
    ctrl->job_group = bot_class_to_group(bot_sd->status.class_);
    ctrl->last_thinktime = gettick();
    ctrl->target_mob_id = 0;
    ctrl->last_action_tick = gettick();

    bot_ctrl_load_rules(ctrl);

    // Seed default rules if empty
    if (ctrl->rules.empty()) {
        BotRule r;

        // Self Heal: bot HP% < 30 → Heal self
        memset(&r, 0, sizeof(r));
        strcpy(r.name, "Self Heal"); r.enabled = true; r.cond_logic = COND_AND;
        r.conditions.push_back({SRC_BOT, COND_HP_PCT, OP_LT, 30, 0});
        r.actions.push_back({ACT_USE_SKILL, 1, 28, ""});
        ctrl->rules.push_back(r);

        // Heal Master: master HP% < 80 → Heal master
        memset(&r, 0, sizeof(r));
        strcpy(r.name, "Heal Master"); r.enabled = true; r.cond_logic = COND_AND;
        r.conditions.push_back({SRC_MASTER, COND_HP_PCT, OP_LT, 80, 0});
        r.actions.push_back({ACT_USE_SKILL, 0, 28, ""});
        ctrl->rules.push_back(r);

        // Blessing: master missing SC_BLESSING → Blessing
        memset(&r, 0, sizeof(r));
        strcpy(r.name, "Blessing"); r.enabled = true; r.cond_logic = COND_AND;
        r.conditions.push_back({SRC_MASTER, COND_SC_MISSING, OP_EQ, 0, 30});
        r.actions.push_back({ACT_USE_SKILL, 0, 34, ""});
        ctrl->rules.push_back(r);

        // Inc AGI: master missing SC_INCREASEAGI → IncAGI
        memset(&r, 0, sizeof(r));
        strcpy(r.name, "Inc AGI"); r.enabled = true; r.cond_logic = COND_AND;
        r.conditions.push_back({SRC_MASTER, COND_SC_MISSING, OP_EQ, 0, 32});
        r.actions.push_back({ACT_USE_SKILL, 0, 29, ""});
        ctrl->rules.push_back(r);

        bot_ctrl_save_rules(ctrl);
    }

    master->bot = ctrl;
    {
        std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
        bot_ctrl_db[master->status.char_id] = ctrl;
    }

    // 修正背包容量和负重，避免旧 Bot DB 数据为 0 导致无法给物品
    if (bot_sd->status.inventory_slots < 10)
        bot_sd->status.inventory_slots = 100;
    bot_sd->max_weight = 200000;

    // 6. 自动入队
    if (master->status.party_id) {
        party_join(*bot_sd, master->status.party_id);
    }

    clif_displaymessage(master->fd, "Bot is online!");
    ShowStatus("Bot '%s' (AID:%d CID:%d) online.\n", st.name, bot_aid, bot_cid);
    return ctrl;
}

/***********************************************************************
 *  创建 Bot
 *  优先查找 DB 中已有的 bot，没有才新建
 ***********************************************************************/
bot_ctrl* bot_ctrl_create(map_session_data* master, int32 class_) {
    nullpo_retr(nullptr, master);

    // 检查内存中是否已有 bot
    bot_ctrl* existing = bot_ctrl_search(master);
    if (existing) {
        if (existing->bot_sd && existing->bot_sd->state.active) {
            clif_displaymessage(master->fd, "Bot is already online!");
            return nullptr;
        }
        if (existing->bot_sd && !existing->bot_sd->state.active) {
            // bot 正在加载中（pc_reg_received 还没到）
            clif_displaymessage(master->fd, "Bot is loading, please wait...");
            return existing;
        }
        // 过期的控制器（no bot_sd），清理掉
        {
            std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
            bot_ctrl_db.erase(master->status.char_id);
        }
        aFree(existing);
    }

    char bot_userid[64];
    snprintf(bot_userid, sizeof(bot_userid), "#BOT_%d", master->status.account_id);
    char esc_userid[128];
    Sql_EscapeString(mmysql_handle, esc_userid, bot_userid);

    // 查找 DB 中是否已有 bot
    char* data;
    int32 bot_aid = 0, bot_cid = 0;

    if (SQL_SUCCESS == Sql_Query(mmysql_handle,
        "SELECT `account_id` FROM `login` WHERE `userid` = '%s'", esc_userid)
    ) {
        if (Sql_NumRows(mmysql_handle) > 0 && SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
            Sql_GetData(mmysql_handle, 0, &data, nullptr);
            bot_aid = atoi(data);
        }
        Sql_FreeResult(mmysql_handle);
    }

    if (bot_aid > 0) {
        // 已有 bot 账号 → 查找角色
        if (SQL_SUCCESS == Sql_Query(mmysql_handle,
            "SELECT `char_id` FROM `char` WHERE `account_id` = %d", bot_aid)
        ) {
            if (Sql_NumRows(mmysql_handle) > 0 && SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
                Sql_GetData(mmysql_handle, 0, &data, nullptr);
                bot_cid = atoi(data);
            }
            Sql_FreeResult(mmysql_handle);
        }

        if (bot_cid > 0) {
            clif_displaymessage(master->fd, "Bot found in DB. Bringing online...");
            return bot_ctrl_bring_online(master, bot_aid, bot_cid);
        }

        // 有账号没有角色 → 删除并重建
        Sql_Query(mmysql_handle, "DELETE FROM `login` WHERE `account_id` = %d", bot_aid);
    }

    // ---- 新建 bot ----
    char bot_name[NAME_LENGTH];
    snprintf(bot_name, NAME_LENGTH, "%s_Bot", master->status.name);
    char esc_name[NAME_LENGTH*2];
    Sql_EscapeString(mmysql_handle, esc_name, bot_name);

    if (SQL_ERROR == Sql_Query(mmysql_handle,
        "INSERT INTO `login` (`userid`,`user_pass`,`sex`,`email`,`group_id`,`state`,`character_slots`) "
        "VALUES ('%s','bot','S','bot@bot',0,0,9)", esc_userid
    )) {
        ShowError("bot_ctrl: DB error login: %s\n", Sql_GetError(mmysql_handle));
        return nullptr;
    }
    bot_aid = (int32)Sql_LastInsertId(mmysql_handle);
    if (bot_aid <= 0) return nullptr;

    if (SQL_ERROR == Sql_Query(mmysql_handle,
        "INSERT INTO `char` (`account_id`,`name`,`class`,`base_level`,`job_level`,"
        "`str`,`agi`,`vit`,`int`,`dex`,`luk`,`max_hp`,`hp`,`max_sp`,`sp`,"
        "`status_point`,`skill_point`,`zeny`,"
        "`last_map`,`last_x`,`last_y`,`save_map`,`save_x`,`save_y`,`sex`,`hair`,`hair_color`,`body`) "
        "VALUES (%d,'%s',%u,1,1,1,1,1,1,1,1,100,100,20,20,0,0,0,"
        "'prontera',156,191,'prontera',156,191,%d,1,0,%u)",
        bot_aid, esc_name, class_, master->status.sex, class_
    )) {
        Sql_Query(mmysql_handle, "DELETE FROM `login` WHERE `account_id` = %d", bot_aid);
        ShowError("bot_ctrl: DB error char: %s\n", Sql_GetError(mmysql_handle));
        return nullptr;
    }
    bot_cid = (int32)Sql_LastInsertId(mmysql_handle);
    if (bot_cid <= 0) {
        Sql_Query(mmysql_handle, "DELETE FROM `login` WHERE `account_id` = %d", bot_aid);
        return nullptr;
    }

    clif_displaymessage(master->fd, "Creating new Bot...");
    bot_ctrl* ctrl = bot_ctrl_bring_online(master, bot_aid, bot_cid);
    if (ctrl && class_ == 4) {
        int32 idx = skill_get_index(NV_BASIC);
        if (idx > 0) {
            ctrl->bot_sd->status.skill[idx].id = NV_BASIC;
            ctrl->bot_sd->status.skill[idx].lv = 9;
            ctrl->bot_sd->status.skill[idx].flag = SKILL_FLAG_PERMANENT;
            clif_displaymessage(master->fd, "NV_BASIC Lv9 granted to Acolyte Bot!");
        }
    }
    return ctrl;
}

/***********************************************************************
 *  销毁
 ***********************************************************************/
void bot_ctrl_destroy(bot_ctrl* ctrl) {
    if (!ctrl) return;
    bot_ctrl_save_rules(ctrl);
    if (ctrl->bot_sd && ctrl->bot_sd->state.active) {
        save_bot_inventory(ctrl->bot_sd);
        chrif_save(ctrl->bot_sd, CSAVE_QUIT);
        map_quit(ctrl->bot_sd);
    }
    if (ctrl->master_sd)
        ctrl->master_sd->bot = nullptr;
    {
        std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
        bot_ctrl_db.erase(ctrl->master_cid);
    }
    aFree(ctrl);
}

/***********************************************************************
 *  初始化/清理
 ***********************************************************************/
void do_init_bot_ctrl(void) {
    add_timer_interval(gettick() + MIN_BOT_THINKTIME, bot_ctrl_ai_timer, 0, 0, MIN_BOT_THINKTIME);

    // Ensure bot_rules table exists
    Sql_Query(mmysql_handle,
        "CREATE TABLE IF NOT EXISTS %s ("
        "master_account_id INT NOT NULL PRIMARY KEY,"
        "rules_json LONGTEXT,"
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB", bot_rules_table);
}
void do_final_bot_ctrl(void) {
    {
        std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
        for (auto it = bot_ctrl_db.begin(); it != bot_ctrl_db.end(); ) {
            bot_ctrl_destroy(it->second);
            it = bot_ctrl_db.begin();
        }
        bot_ctrl_db.clear();
    }
}

/***********************************************************************
 *  HTTP API - return JSON string for bot status
 ***********************************************************************/
static std::string bot_api_json_status(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    if (!bot) return "{\"code\":-1,\"msg\":\"no bot\"}";

    auto& st = bot->status;
    auto& bs = bot->battle_status;
    const char* map_name = mapindex_id2name(bot->mapindex);

    // Build skill list JSON
    std::string skills_json;
    skills_json += "[";
    bool first = true;
    for (int i = 0; i < MAX_SKILL; i++) {
        if (st.skill[i].id != 0 && st.skill[i].lv > 0) {
            if (!first) skills_json += ",";
            first = false;
            const char* sname = skill_get_name(st.skill[i].id);
            if (!sname) sname = "";
            // Escape backslash and quotes for JSON
            std::string esc_name;
            for (const char* p = sname; *p; p++) {
                if (*p == '\\' || *p == '"') { esc_name += '\\'; }
                esc_name += *p;
            }
            char sk[256];
            snprintf(sk, sizeof(sk), "{\"id\":%u,\"lv\":%u,\"name\":\"%s\"}",
                st.skill[i].id, st.skill[i].lv, esc_name.c_str());
            skills_json += sk;
        }
    }
    skills_json += "]";

    t_exp next_base = pc_nextbaseexp(bot);
    t_exp next_job  = pc_nextjobexp(bot);

    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{"
        "\"code\":0,"
        "\"msg\":\"ok\","
        "\"data\":{"
        "\"bot_name\":\"%s\","
        "\"class\":%d,"
        "\"base_level\":%d,"
        "\"job_level\":%d,"
        "\"base_exp\":%llu,"
        "\"next_base_exp\":%llu,"
        "\"job_exp\":%llu,"
        "\"next_job_exp\":%llu,"
        "\"hp\":%d,"
        "\"max_hp\":%d,"
        "\"sp\":%d,"
        "\"max_sp\":%d,"
        "\"str\":%d,\"agi\":%d,\"vit\":%d,\"int\":%d,\"dex\":%d,\"luk\":%d,"
        "\"map\":\"%s\","
        "\"x\":%d,\"y\":%d,"
        "\"ai_mode\":%d,"
        "\"target_mob_id\":%d,"
        "\"zeny\":%d,"
        "\"status_point\":%d,"
        "\"skill_point\":%d,"
        "\"skills\":%s"
        "}"
        "}",
        st.name,
        st.class_,
        st.base_level,
        st.job_level,
        (unsigned long long)st.base_exp,
        (unsigned long long)next_base,
        (unsigned long long)st.job_exp,
        (unsigned long long)next_job,
        bs.hp,
        bs.max_hp,
        bs.sp,
        bs.max_sp,
        st.str, st.agi, st.vit, st.int_, st.dex, st.luk,
        map_name ? map_name : "unknown",
        bot->x, bot->y,
        (int)ctrl->ai_mode,
        ctrl->target_mob_id,
        st.zeny,
        st.status_point,
        st.skill_point,
        skills_json.c_str()
    );
    return std::string(buf);
}

/***********************************************************************
 *  HTTP API handlers
 ***********************************************************************/
static httplib::Server* bot_api_svr = nullptr;
static std::thread* bot_api_thread = nullptr;
static bool bot_api_running = false;

static void bot_api_handle_status(const httplib::Request& req, httplib::Response& res) {
    auto aid = req.get_param_value("aid");
    if (aid.empty()) {
        res.set_content("{\"code\":-1,\"msg\":\"missing aid\"}", "application/json");
        return;
    }
    int32 account_id = std::stoi(aid);

    std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
    for (auto& it : bot_ctrl_db) {
        if (it.second->master_aid == account_id) {
            res.set_content(bot_api_json_status(it.second), "application/json");
            return;
        }
    }
    res.set_content("{\"code\":-1,\"msg\":\"bot not found\"}", "application/json");
}

static void bot_api_handle_cmd(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = nlohmann::json::parse(req.body);
        int32 aid = body.value("aid", 0);
        std::string cmd = body.value("cmd", "");
        if (aid == 0 || cmd.empty()) {
            res.set_content("{\"code\":-1,\"msg\":\"missing aid or cmd\"}", "application/json");
            return;
        }

        bot_ctrl* ctrl = nullptr;
        {
            std::lock_guard<std::mutex> lock(bot_ctrl_db_mutex);
            for (auto& it : bot_ctrl_db) {
                if (it.second->master_aid == aid) {
                    ctrl = it.second;
                    break;
                }
            }
        }

        if (!ctrl) {
            res.set_content("{\"code\":-1,\"msg\":\"bot not found\"}", "application/json");
            return;
        }

        auto bot = ctrl->bot_sd;
        auto sd = ctrl->master_sd;
        if (!bot || !sd) {
            res.set_content("{\"code\":-1,\"msg\":\"bot session invalid\"}", "application/json");
            return;
        }

        if (cmd == "ai_mode") {
            int mode = body["params"].value("mode", -1);
            if (mode >= 0 && mode < AI_MODE_MAX) {
                ctrl->ai_mode = (bot_ai_mode)mode;
                if (sd->fd)
                    clif_displaymessage(sd->fd, "Bot mode changed via web!");
                res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
            } else {
                res.set_content("{\"code\":-1,\"msg\":\"invalid mode\"}", "application/json");
            }
        } else if (cmd == "recall") {
            if (bot->prev != nullptr)
                unit_remove_map(bot, CLR_OUTSIGHT);
            bot->mapindex = sd->mapindex;
            bot->m = sd->m;
            bot->x = sd->x + 1;
            bot->y = sd->y;
            map_addblock(bot);
            clif_spawn(bot);
            if (sd->fd)
                clif_displaymessage(sd->fd, "Bot recalled via web!");
            res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
        } else if (cmd == "revive") {
            if (bot->status.hp <= 0) {
                status_revive(bot, 100, 100);
                clif_spawn(bot);
                if (sd->fd)
                    clif_displaymessage(sd->fd, "Bot revived via web!");
            }
            res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
        } else if (cmd == "destroy") {
            if (sd->fd)
                clif_displaymessage(sd->fd, "Bot destroyed via web!");
            bot_ctrl_destroy(ctrl);
            res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
        } else if (cmd == "statusup") {
            std::string stat = body["params"].value("stat", "");
            int amount = body["params"].value("amount", 0);
            int sp_type = -1;
            if (stat == "str") sp_type = SP_STR;
            else if (stat == "agi") sp_type = SP_AGI;
            else if (stat == "vit") sp_type = SP_VIT;
            else if (stat == "int") sp_type = SP_INT;
            else if (stat == "dex") sp_type = SP_DEX;
            else if (stat == "luk") sp_type = SP_LUK;
            if (sp_type < 0 || amount <= 0) {
                res.set_content("{\"code\":-1,\"msg\":\"invalid stat or amount\"}", "application/json");
            } else if ((int32)bot->status.status_point < amount) {
                res.set_content("{\"code\":-1,\"msg\":\"not enough status points\"}", "application/json");
            } else if (!pc_statusup(bot, sp_type, amount)) {
                res.set_content("{\"code\":-1,\"msg\":\"stat up failed (max reached?)\"}", "application/json");
            } else {
                if (sd->fd)
                    clif_displaymessage(sd->fd, "Bot stats increased via web!");
                res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
            }
        } else if (cmd == "get_skills") {
            nlohmann::json arr = nlohmann::json::array();
            for (int i = 0; i < MAX_SKILL; i++) {
                if (bot->status.skill[i].id != 0 && bot->status.skill[i].lv > 0) {
                    nlohmann::json sk;
                    sk["id"] = bot->status.skill[i].id;
                    sk["lv"] = bot->status.skill[i].lv;
                    arr.push_back(sk);
                }
            }
            nlohmann::json j;
            j["code"] = 0;
            j["data"] = arr;
            res.set_content(j.dump(), "application/json");
        } else if (cmd == "learn_skill") {
            uint16 skill_id = (uint16)body["params"].value("skill_id", 0);
            if (skill_id == 0) {
                res.set_content("{\"code\":-1,\"msg\":\"invalid skill_id\"}", "application/json");
                return;
            }
            auto entry = skill_tree_db.get_skill_data(bot->status.class_, skill_id);
            if (!entry) {
                res.set_content("{\"code\":-1,\"msg\":\"your bot cannot learn this skill\"}", "application/json");
                return;
            }
            uint8 cur_lv = pc_checkskill(bot, skill_id);
            if (cur_lv >= entry->max_lv) {
                res.set_content("{\"code\":-1,\"msg\":\"already max level\"}", "application/json");
                return;
            }
            if (bot->status.base_level < (int32)entry->baselv || bot->status.job_level < (int32)entry->joblv) {
                res.set_content("{\"code\":-1,\"msg\":\"level too low\"}", "application/json");
                return;
            }
            // Check prerequisites
            for (auto& prereq : entry->need) {
                uint16 pre_id = prereq.first;
                uint16 pre_lv = prereq.second;
                if (pc_checkskill(bot, pre_id) < pre_lv) {
                    char err[256];
                    snprintf(err, sizeof(err), "need prerequisite skill %u level %u", pre_id, pre_lv);
                    res.set_content((std::string("{\"code\":-1,\"msg\":\"") + err + "\"}").c_str(), "application/json");
                    return;
                }
            }
            if (bot->status.skill_point < 1) {
                res.set_content("{\"code\":-1,\"msg\":\"not enough skill points\"}", "application/json");
                return;
            }
            pc_skill(bot, skill_id, cur_lv + 1, ADDSKILL_PERMANENT);
            bot->status.skill_point--;
            if (sd->fd)
                clif_displaymessage(sd->fd, "Bot skill learned via web!");
            res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
        } else if (cmd == "get_class_skills") {
            // Build inheritance chain (base → current)
            std::vector<int32> chain;
            int32 cid = bot->status.class_;
            while (cid >= 0) {
                chain.push_back(cid);
                auto tr = skill_tree_db.find(cid);
                if (!tr || tr->inherit_job.empty()) break;
                cid = tr->inherit_job[0];
            }
            std::reverse(chain.begin(), chain.end());

            nlohmann::json result = nlohmann::json::array();
            std::set<uint16> seen;

            for (int32 job_id : chain) {
                auto tr = skill_tree_db.find(job_id);
                if (!tr) continue;

                nlohmann::json group;
                group["job_id"] = job_id;
                group["job_name"] = job_name(job_id);
                nlohmann::json skills = nlohmann::json::array();

                // Collect skills sorted by id
                std::vector<uint16> sorted_skids;
                for (auto& it : tr->skills) {
                    if (seen.count(it.first)) continue;
                    sorted_skids.push_back(it.first);
                }
                std::sort(sorted_skids.begin(), sorted_skids.end());

                for (uint16 skid : sorted_skids) {
                    seen.insert(skid);
                    auto& entry = tr->skills[skid];
                    nlohmann::json sk;
                    sk["id"] = skid;
                    sk["name"] = skill_get_name(skid) ? skill_get_name(skid) : "";
                    sk["max_lv"] = entry->max_lv;
                    sk["baselv"] = entry->baselv;
                    sk["joblv"]  = entry->joblv;
                    sk["cur_lv"] = pc_checkskill(bot, skid);

                    nlohmann::json prereqs = nlohmann::json::array();
                    for (auto& [pid, plv] : entry->need) {
                        prereqs.push_back({
                            {"id", pid},
                            {"lv", plv},
                            {"cur_lv", pc_checkskill(bot, pid)}
                        });
                    }
                    sk["prereqs"] = prereqs;
                    skills.push_back(sk);
                }
                group["skills"] = skills;
                result.push_back(group);
            }

            nlohmann::json jr;
            jr["code"] = 0; jr["data"] = result;
            res.set_content(jr.dump(), "application/json");
        } else if (cmd == "get_rules") {
            nlohmann::json arr = nlohmann::json::array();
            for (auto& r : ctrl->rules) {
                nlohmann::json jr;
                jr["name"] = r.name;
                jr["enabled"] = r.enabled;
                jr["cond_logic"] = r.cond_logic;
                nlohmann::json jc = nlohmann::json::array();
                for (auto& c : r.conditions) {
                    nlohmann::json jcc;
                    jcc["source"] = c.source; jcc["type"] = c.type;
                    jcc["op"] = c.op; jcc["value"] = c.value; jcc["extra"] = c.extra;
                    jc.push_back(jcc);
                }
                jr["conditions"] = jc;
                nlohmann::json ja = nlohmann::json::array();
                for (auto& a : r.actions) {
                    nlohmann::json jac;
                    jac["type"] = a.type; jac["target"] = a.target;
                    jac["skill_id"] = a.skill_id; jac["message"] = a.message;
                    ja.push_back(jac);
                }
                jr["actions"] = ja;
                arr.push_back(jr);
            }
            nlohmann::json jr;
            jr["code"] = 0; jr["data"] = arr;
            res.set_content(jr.dump(), "application/json");
        } else if (cmd == "save_rules") {
            auto& rules_json = body["params"]["rules"];
            ctrl->rules.clear();
            for (auto& jr : rules_json) {
                BotRule r;
                std::string n = jr.value("name", "");
                strncpy(r.name, n.c_str(), RULE_NAME_LEN - 1);
                r.name[RULE_NAME_LEN - 1] = '\0';
                r.enabled = jr.value("enabled", true);
                r.cond_logic = jr.value("cond_logic", 0);
                for (auto& jc : jr.value("conditions", nlohmann::json::array())) {
                    BotRuleCond c;
                    c.source = jc.value("source", 0);
                    c.type   = jc.value("type", 0);
                    c.op     = jc.value("op", 0);
                    c.value  = jc.value("value", 0);
                    c.extra  = jc.value("extra", 0);
                    r.conditions.push_back(c);
                }
                for (auto& ja : jr.value("actions", nlohmann::json::array())) {
                    BotRuleAction a;
                    memset(&a, 0, sizeof(a));
                    a.type     = ja.value("type", 0);
                    a.target   = ja.value("target", 0);
                    a.skill_id = ja.value("skill_id", 0);
                    std::string msg = ja.value("message", "");
                    strncpy(a.message, msg.c_str(), 63);
                    a.message[63] = '\0';
                    r.actions.push_back(a);
                }
                ctrl->rules.push_back(r);
            }
            bot_ctrl_save_rules(ctrl);
            if (sd->fd)
                clif_displaymessage(sd->fd, "Bot rules saved via web!");
            res.set_content("{\"code\":0,\"msg\":\"ok\"}", "application/json");
        } else {
            res.set_content("{\"code\":-1,\"msg\":\"unknown cmd\"}", "application/json");
        }
    } catch (const std::exception& e) {
        res.set_content("{\"code\":-1,\"msg\":\"parse error:" + std::string(e.what()) + "\"}", "application/json");
    }
}

/***********************************************************************
 *  HTTP API init/final
 ***********************************************************************/
void do_init_bot_http_api(void) {
    bot_api_svr = new httplib::Server();

    bot_api_svr->Get("/api/bot/status", bot_api_handle_status);
    bot_api_svr->Post("/api/bot/cmd", bot_api_handle_cmd);

    bot_api_running = true;
    bot_api_thread = new std::thread([] {
        bot_api_svr->listen("127.0.0.1", BOT_API_DEFAULT_PORT);
    });
    bot_api_thread->detach();

    ShowStatus("Bot HTTP API listening on 127.0.0.1:%d\n", BOT_API_DEFAULT_PORT);
}

void do_final_bot_http_api(void) {
    if (bot_api_svr) {
        bot_api_running = false;
        bot_api_svr->stop();
        delete bot_api_svr;
        bot_api_svr = nullptr;
    }
}
