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

struct bot_ctrl {
    map_session_data* bot_sd;
    map_session_data* master_sd;
    int32 master_aid, master_cid;
    int32 bot_aid, bot_cid;

    enum : uint8 {
        AI_FOLLOW,
        AI_SUPPORT,
        AI_STANDBY
    } ai_state;

    struct {
        uint8 hp_threshold;
        bool auto_buff;
        bool auto_heal;
    } config;

    t_tick last_thinktime;
};

bot_ctrl* bot_ctrl_search(map_session_data* master);
bot_ctrl* bot_ctrl_create(map_session_data* master, int32 class_ = 0);
void bot_ctrl_destroy(bot_ctrl* ctrl);

void do_init_bot_ctrl(void);
void do_final_bot_ctrl(void);

#endif
