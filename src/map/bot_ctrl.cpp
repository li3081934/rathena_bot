#include "bot_ctrl.hpp"

#include <common/malloc.hpp>
#include <common/nullpo.hpp>
#include <common/showmsg.hpp>
#include <common/sql.hpp>

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

#include <unordered_map>
static std::unordered_map<int32, bot_ctrl*> bot_ctrl_db;

extern Sql* mmysql_handle;

/***********************************************************************
 *  查找
 ***********************************************************************/
bot_ctrl* bot_ctrl_search(map_session_data* master) {
    if (!master) return nullptr;
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
 *  AI - Common Behaviours (fallback for all jobs)
 ***********************************************************************/

static void common_follow(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!sd || !bot) return;
    if (check_distance_bl(sd, bot, 3)) return;
    if (DIFF_TICK(gettick(), bot->ud.canmove_tick) < 0) return;
    unit_walktobl(bot, sd, 2, 0);
}

static void common_standby(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    if (!bot) return;
    // Self-heal if HP < 30%
    if (ctrl->config.auto_heal && bot->status.hp > 0) {
        uint8 hp_per = (uint8)((uint64)bot->battle_status.hp * 100 / bot->battle_status.max_hp);
        if (hp_per < 30) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, bot->id, AL_HEAL, lv);
                return;
            }
        }
    }
}

static void common_support(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return;
    t_tick tick = gettick();

    // Heal master if HP low
    if (ctrl->config.auto_heal && sd->status.hp > 0
        && DIFF_TICK(tick, ctrl->last_action_tick) > 1000) {
        uint8 hp_per = (uint8)((uint64)sd->battle_status.hp * 100 / sd->battle_status.max_hp);
        if (hp_per < ctrl->config.hp_threshold) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, sd->id, AL_HEAL, lv);
                ctrl->last_action_tick = tick;
                return;
            }
        }
    }

    common_follow(ctrl);
}

static void common_guard(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return;
    t_tick tick = gettick();

    // Heal master if needed
    if (ctrl->config.auto_heal && sd->status.hp > 0
        && DIFF_TICK(tick, ctrl->last_action_tick) > 1000) {
        uint8 hp_per = (uint8)((uint64)sd->battle_status.hp * 100 / sd->battle_status.max_hp);
        if (hp_per < ctrl->config.hp_threshold) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, sd->id, AL_HEAL, lv);
                ctrl->last_action_tick = tick;
                return;
            }
        }
    }

    common_follow(ctrl);
}

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

static void common_assault(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return;

    block_list* mob = bot_find_nearest_mob(bot, 8);
    if (mob) {
        unit_attack(bot, mob->id, 1);
        ctrl->target_mob_id = mob->id;
        return;
    }

    common_follow(ctrl);
}

/***********************************************************************
 *  AI - Acolyte-specific behaviours
 ***********************************************************************/

static void acolyte_support(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return;
    t_tick tick = gettick();

    if (DIFF_TICK(tick, ctrl->last_action_tick) < 500)
        goto _follow;

    // 1. Self-heal if HP < 30%
    if (bot->status.hp > 0) {
        uint8 my_hp = (uint8)((uint64)bot->battle_status.hp * 100 / bot->battle_status.max_hp);
        if (my_hp < 30) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, bot->id, AL_HEAL, lv);
                ctrl->last_action_tick = tick;
                return;
            }
        }
    }

    // 2. Heal master
    if (ctrl->config.auto_heal && sd->status.hp > 0 && sd->battle_status.max_hp > 0) {
        uint8 hp_per = (uint8)((uint64)sd->battle_status.hp * 100 / sd->battle_status.max_hp);
        if (hp_per < ctrl->config.hp_threshold) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, sd->id, AL_HEAL, lv);
                ctrl->last_action_tick = tick;
                return;
            }
        }
    }

    // 3. Buffs
    if (ctrl->config.auto_buff) {
        uint8 lv;
        lv = pc_checkskill(bot, AL_BLESSING);
        if (lv > 0 && !sd->sc.getSCE(SC_BLESSING)
            && bot->battle_status.sp >= skill_get_sp(AL_BLESSING, lv)) {
            unit_skilluse_id(bot, sd->id, AL_BLESSING, lv);
            ctrl->last_action_tick = tick;
            return;
        }
        lv = pc_checkskill(bot, AL_INCAGI);
        if (lv > 0 && !sd->sc.getSCE(SC_INCREASEAGI)
            && bot->battle_status.sp >= skill_get_sp(AL_INCAGI, lv)) {
            unit_skilluse_id(bot, sd->id, AL_INCAGI, lv);
            ctrl->last_action_tick = tick;
            return;
        }
        lv = pc_checkskill(bot, PR_KYRIE);
        if (lv > 0 && !sd->sc.getSCE(SC_KYRIE)
            && bot->battle_status.sp >= skill_get_sp(PR_KYRIE, lv)) {
            unit_skilluse_id(bot, sd->id, PR_KYRIE, lv);
            ctrl->last_action_tick = tick;
            return;
        }
        lv = pc_checkskill(bot, HP_ASSUMPTIO);
        if (lv > 0 && !sd->sc.getSCE(SC_ASSUMPTIO)
            && bot->battle_status.sp >= skill_get_sp(HP_ASSUMPTIO, lv)) {
            unit_skilluse_id(bot, sd->id, HP_ASSUMPTIO, lv);
            ctrl->last_action_tick = tick;
            return;
        }
    }

_follow:
    common_follow(ctrl);
}

static void acolyte_guard(bot_ctrl* ctrl) {
    auto bot = ctrl->bot_sd;
    auto sd = ctrl->master_sd;
    if (!bot || !sd) return;
    t_tick tick = gettick();

    // 1. Self-heal if HP < 30%
    if (bot->status.hp > 0) {
        uint8 my_hp = (uint8)((uint64)bot->battle_status.hp * 100 / bot->battle_status.max_hp);
        if (my_hp < 30) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, bot->id, AL_HEAL, lv);
                ctrl->last_action_tick = tick;
                return;
            }
        }
    }

    // 2. Revive master
    if (sd->status.hp <= 0 && sd->status.char_id > 0) {
        uint8 lv = pc_checkskill(bot, ALL_RESURRECTION);
        if (lv > 0 && bot->battle_status.sp >= skill_get_sp(ALL_RESURRECTION, lv)) {
            unit_skilluse_id(bot, sd->id, ALL_RESURRECTION, lv);
            ctrl->last_action_tick = tick;
            return;
        }
    }

    // 3. Heal master
    if (ctrl->config.auto_heal && sd->status.hp > 0 && sd->battle_status.max_hp > 0) {
        uint8 hp_per = (uint8)((uint64)sd->battle_status.hp * 100 / sd->battle_status.max_hp);
        if (hp_per < ctrl->config.hp_threshold) {
            uint8 lv = pc_checkskill(bot, AL_HEAL);
            if (lv > 0 && bot->battle_status.sp >= skill_get_sp(AL_HEAL, lv)) {
                unit_skilluse_id(bot, sd->id, AL_HEAL, lv);
                ctrl->last_action_tick = tick;
                return;
            }
        }
    }

    common_follow(ctrl);
}

/***********************************************************************
 *  AI - Job dispatch table
 ***********************************************************************/

static bot_mode_action_fn job_mode_table[JOBGROUP_MAX][AI_MODE_MAX] = {
    /* NOVICE   */ { common_follow, common_support, common_standby, common_guard,   common_assault },
    /* SWORDMAN */ { common_follow, common_support, common_standby, common_guard,   common_assault },
    /* MAGE     */ { common_follow, common_support, common_standby, common_guard,   common_assault },
    /* ARCHER   */ { common_follow, common_support, common_standby, common_guard,   common_assault },
    /* ACOLYTE  */ { common_follow, acolyte_support, common_standby, acolyte_guard, common_assault },
    /* MERCHANT */ { common_follow, common_support, common_standby, common_guard,   common_assault },
    /* THIEF    */ { common_follow, common_support, common_standby, common_guard,   common_assault },
};

/***********************************************************************
 *  AI - Main entry
 ***********************************************************************/

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

    job_mode_table[ctrl->job_group][ctrl->ai_mode](ctrl);
    return 0;
}

static int32 bot_ctrl_ai_foreach(map_session_data* sd, va_list ap) {
    t_tick tick = va_arg(ap, t_tick);
    auto it = bot_ctrl_db.find(sd->status.char_id);
    if (it != bot_ctrl_db.end())
        bot_ctrl_ai_sub(it->second, tick);
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
    ctrl->ai_mode = AI_SUPPORT;
    ctrl->job_group = bot_class_to_group(bot_sd->status.class_);
    ctrl->config.hp_threshold = 80;
    ctrl->config.sp_threshold = 20;
    ctrl->config.auto_buff = true;
    ctrl->config.auto_heal = true;
    ctrl->config.auto_loot = true;
    ctrl->last_thinktime = gettick();
    ctrl->target_mob_id = 0;
    ctrl->last_action_tick = gettick();

    master->bot = ctrl;
    bot_ctrl_db[master->status.char_id] = ctrl;

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
        bot_ctrl_db.erase(master->status.char_id);
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
    if (ctrl->bot_sd && ctrl->bot_sd->state.active) {
        save_bot_inventory(ctrl->bot_sd);
        chrif_save(ctrl->bot_sd, CSAVE_QUIT);
        map_quit(ctrl->bot_sd);
    }
    if (ctrl->master_sd)
        ctrl->master_sd->bot = nullptr;
    bot_ctrl_db.erase(ctrl->master_cid);
    aFree(ctrl);
}

/***********************************************************************
 *  初始化/清理
 ***********************************************************************/
void do_init_bot_ctrl(void) {
    add_timer_interval(gettick() + MIN_BOT_THINKTIME, bot_ctrl_ai_timer, 0, 0, MIN_BOT_THINKTIME);
}
void do_final_bot_ctrl(void) {
    for (auto it = bot_ctrl_db.begin(); it != bot_ctrl_db.end(); ) {
        bot_ctrl_destroy(it->second);
        it = bot_ctrl_db.begin();
    }
    bot_ctrl_db.clear();
}
