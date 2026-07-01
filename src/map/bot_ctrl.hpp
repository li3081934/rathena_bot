#ifndef BOT_CTRL_HPP
#define BOT_CTRL_HPP

#include <common/cbasetypes.hpp>
#include <common/mmo.hpp>
#include <common/timer.hpp>

#include "map.hpp"
#include "pc.hpp"
#include "status.hpp"
#include "unit.hpp"

#define MIN_BOT_THINKTIME 500

// 5 behaviour modes
enum bot_ai_mode : uint8 {
    AI_FOLLOW,      // 0 - follow master
    AI_STANDBY,     // 1 - stay put
    AI_SUPPORT,     // 2 - follow + buff + heal + assist
    AI_GUARD,       // 3 - protect master from attackers
    AI_ASSAULT,     // 4 - actively attack nearby monsters
    AI_MODE_MAX
};

// Job groups (grouping classes with similar playstyle)
enum bot_job_group : uint8 {
    JOBGROUP_NOVICE,
    JOBGROUP_SWORDMAN,
    JOBGROUP_MAGE,
    JOBGROUP_ARCHER,
    JOBGROUP_ACOLYTE,
    JOBGROUP_MERCHANT,
    JOBGROUP_THIEF,
    JOBGROUP_MAX
};

struct bot_ctrl;

// Function pointer for a single mode's AI tick
typedef void (*bot_mode_action_fn)(bot_ctrl*);

struct bot_ctrl {
    map_session_data* bot_sd;
    map_session_data* master_sd;
    int32 master_aid, master_cid;
    int32 bot_aid, bot_cid;

    bot_ai_mode ai_mode;
    bot_job_group job_group;

    struct {
        uint8 hp_threshold;
        uint8 sp_threshold;
        bool auto_buff;
        bool auto_heal;
        bool auto_loot;
    } config;

    t_tick last_thinktime;
    int32 target_mob_id;
    t_tick last_action_tick;
};

bot_ctrl* bot_ctrl_search(map_session_data* master);
bot_ctrl* bot_ctrl_create(map_session_data* master, int32 class_ = 0);
void bot_ctrl_destroy(bot_ctrl* ctrl);

void do_init_bot_ctrl(void);
void do_final_bot_ctrl(void);

// Exposed for script.inc (bot_ai_mode)
bot_job_group bot_class_to_group(int32 class_);

#endif
