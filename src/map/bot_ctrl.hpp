#ifndef BOT_CTRL_HPP
#define BOT_CTRL_HPP

#include <mutex>
#include <string>
#include <vector>

#include <common/cbasetypes.hpp>
#include <common/mmo.hpp>
#include <common/timer.hpp>

#include "map.hpp"
#include "pc.hpp"
#include "status.hpp"
#include "unit.hpp"

#define MIN_BOT_THINKTIME 500
#define BOT_API_DEFAULT_PORT 5122
#define MAX_RULES 32
#define MAX_CONDITIONS 8
#define MAX_ACTIONS 8
#define RULE_NAME_LEN 48

// 3 behaviour modes (rest handled by rule engine)
enum bot_ai_mode : uint8 {
    AI_ACTIVE,      // 0 - execute rule engine, follow master when no rule fires
    AI_PASSIVE,     // 1 - skip rule engine, only follow master
    AI_STANDBY,     // 2 - stay put, do nothing
    AI_MODE_MAX
};

// Job groups
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

// --- Rule engine types ---
enum bot_cond_logic : uint8 {
    COND_AND = 0,   // all conditions must match
    COND_OR  = 1    // any condition matches
};

enum bot_cond_op : uint8 {
    OP_LT, OP_GT, OP_EQ, OP_NE, OP_LE, OP_GE
};

enum bot_cond_source : uint8 {
    SRC_MASTER = 0,
    SRC_BOT = 1,
    SRC_ENV = 2
};

enum bot_cond_type : uint8 {
    COND_HP_PCT,
    COND_SP_PCT,
    COND_SC_MISSING,
    COND_SC_ACTIVE,
    COND_IS_DEAD,
    COND_DIST_GT,
    COND_DIST_LT,
    COND_HAS_ENEMY
};

enum bot_action_type : uint8 {
    ACT_USE_SKILL,
    ACT_ATTACK_NEAREST,
    ACT_RECALL,
    ACT_SAY
};

struct BotRuleCond {
    uint8 source;      // SRC_MASTER / SRC_BOT / SRC_ENV
    uint8 type;        // COND_HP_PCT / etc
    uint8 op;          // OP_LT / GT / EQ / etc
    int32 value;       // threshold
    uint16 extra;      // SC_ constant / skill_id
};

struct BotRuleAction {
    uint8 type;        // ACT_USE_SKILL / ACT_ATTACK_NEAREST / etc
    uint8 target;      // 0=master, 1=bot, 2=self
    uint16 skill_id;   // for USE_SKILL
    char message[64];  // for SAY
};

struct BotRule {
    char name[RULE_NAME_LEN];
    bool enabled;
    uint8 cond_logic;  // COND_AND / COND_OR
    std::vector<BotRuleCond> conditions;
    std::vector<BotRuleAction> actions;
};

struct bot_ctrl;

struct bot_ctrl {
    map_session_data* bot_sd;
    map_session_data* master_sd;
    int32 master_aid, master_cid;
    int32 bot_aid, bot_cid;

    bot_ai_mode ai_mode;
    bot_job_group job_group;

    std::vector<BotRule> rules;

    t_tick last_thinktime;
    int32 target_mob_id;
    t_tick last_action_tick;
};

bot_ctrl* bot_ctrl_search(map_session_data* master);
bot_ctrl* bot_ctrl_create(map_session_data* master, int32 class_ = 0);
void bot_ctrl_destroy(bot_ctrl* ctrl);

void do_init_bot_ctrl(void);
void do_final_bot_ctrl(void);

void do_init_bot_http_api(void);
void do_final_bot_http_api(void);

bot_job_group bot_class_to_group(int32 class_);

#endif
