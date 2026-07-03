// Bot System - new: uses real map_session_data
BUILDIN_FUNC(bot_create)
{
    map_session_data* sd;
    if (!script_rid2sd(sd))
        return SCRIPT_CMD_SUCCESS;

    int32 class_ = script_getnum(st, 2);
    bot_ctrl* ctrl = bot_ctrl_create(sd, class_);
    script_pushint(st, ctrl ? 1 : 0);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_destroy)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot)
        return SCRIPT_CMD_SUCCESS;

    bot_ctrl_destroy(sd->bot);
    sd->bot = nullptr;
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_getinfo)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd) {
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    map_session_data* bot = sd->bot->bot_sd;
    const char* field = script_getstr(st, 2);
    int64 val = 0;

    if (strcmp(field, "base_level") == 0)        val = bot->status.base_level;
    else if (strcmp(field, "job_level") == 0)     val = bot->status.job_level;
    else if (strcmp(field, "str") == 0)          val = bot->status.str;
    else if (strcmp(field, "agi") == 0)          val = bot->status.agi;
    else if (strcmp(field, "vit") == 0)          val = bot->status.vit;
    else if (strcmp(field, "int") == 0)          val = bot->status.int_;
    else if (strcmp(field, "dex") == 0)          val = bot->status.dex;
    else if (strcmp(field, "luk") == 0)          val = bot->status.luk;
    else if (strcmp(field, "hp") == 0)           val = bot->battle_status.hp;
    else if (strcmp(field, "max_hp") == 0)       val = bot->battle_status.max_hp;
    else if (strcmp(field, "sp") == 0)           val = bot->battle_status.sp;
    else if (strcmp(field, "max_sp") == 0)       val = bot->battle_status.max_sp;
    else if (strcmp(field, "status_point") == 0) val = bot->status.status_point;
    else if (strcmp(field, "skill_point") == 0)  val = bot->status.skill_point;
    else if (strcmp(field, "class") == 0)        val = bot->status.class_;
    else if (strcmp(field, "ai_state") == 0)     val = sd->bot->ai_mode;
    else ShowWarning("bot_getinfo: unknown field '%s'.\n", field);

    script_pushint(st, val);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_statusup)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    int32 type = script_getnum(st, 2);
    int32 amount = script_getnum(st, 3);

    switch (type) {
    case SP_STR: pc_statusup(bot, SP_STR, amount); break;
    case SP_AGI: pc_statusup(bot, SP_AGI, amount); break;
    case SP_VIT: pc_statusup(bot, SP_VIT, amount); break;
    case SP_INT: pc_statusup(bot, SP_INT, amount); break;
    case SP_DEX: pc_statusup(bot, SP_DEX, amount); break;
    case SP_LUK: pc_statusup(bot, SP_LUK, amount); break;
    default: return SCRIPT_CMD_SUCCESS;
    }
    return SCRIPT_CMD_SUCCESS;
}

// 递归计算技能链总需求 SP（包括所有前置）
// depth 防止环路
static int32 bot_skill_chain_cost(map_session_data* bot, int32 class_, uint16 skill_id, uint8 target_lv, int32 depth) {
    if (depth > 20) return -1;

    auto entry = skill_tree_db.get_skill_data(class_, skill_id);
    if (entry == nullptr) return -1;

    uint8 cur_lv = pc_checkskill(bot, skill_id);
    uint8 need_lv = (target_lv > cur_lv) ? (target_lv - cur_lv) : 0;

    int32 total = need_lv; // 每级消耗 1 SP

    for (auto& prereq : entry->need) {
        uint16 pre_id = prereq.first;
        uint16 pre_lv = prereq.second;
        uint8 pre_cur = pc_checkskill(bot, pre_id);
        if (pre_cur >= pre_lv) continue;

        int32 sub = bot_skill_chain_cost(bot, class_, pre_id, (uint8)pre_lv, depth + 1);
        if (sub < 0) return -1;
        total += sub;
    }

    return total;
}

// 递归学习技能链
static bool bot_skill_learn_chain(map_session_data* bot, int32 class_, uint16 skill_id, uint8 target_lv, int32 depth) {
    if (depth > 20) return false;

    auto entry = skill_tree_db.get_skill_data(class_, skill_id);
    if (entry == nullptr) return false;

    // 先学前置
    for (auto& prereq : entry->need) {
        uint16 pre_id = prereq.first;
        uint16 pre_lv = prereq.second;
        if (pc_checkskill(bot, pre_id) >= pre_lv) continue;

        if (!bot_skill_learn_chain(bot, class_, pre_id, (uint8)pre_lv, depth + 1))
            return false;
    }

    // 学当前技能（只提升到目标等级中未学的部分）
    uint8 cur_lv = pc_checkskill(bot, skill_id);
    if (target_lv > cur_lv) {
        for (uint8 lv = cur_lv + 1; lv <= target_lv; lv++) {
            pc_skill(bot, skill_id, lv, ADDSKILL_PERMANENT);
        }
    }

    return true;
}

BUILDIN_FUNC(bot_skill)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    uint16 skill_id = (uint16)script_getnum(st, 2);
    int32 bot_class = bot->status.class_;

    // 检查职业技能树
    auto entry = skill_tree_db.get_skill_data(bot_class, skill_id);
    if (entry == nullptr) {
        clif_displaymessage(sd->fd, "Your Bot cannot learn this skill!");
        return SCRIPT_CMD_SUCCESS;
    }

    // 每次升 1 级
    uint8 cur_lv = pc_checkskill(bot, skill_id);
    uint8 target_lv = cur_lv + 1;

    if (target_lv > entry->max_lv) {
        clif_displaymessage(sd->fd, "Already max level!");
        return SCRIPT_CMD_SUCCESS;
    }

    // 检查等级要求
    if (bot->status.base_level < entry->baselv || bot->status.job_level < entry->joblv) {
        clif_displaymessage(sd->fd, "Level too low for this skill!");
        return SCRIPT_CMD_SUCCESS;
    }

    // 计算总 SP 需求（包括所有前置链）
    int32 total_sp = bot_skill_chain_cost(bot, bot_class, skill_id, target_lv, 0);
    if (total_sp < 0) {
        clif_displaymessage(sd->fd, "Cannot learn this skill!");
        return SCRIPT_CMD_SUCCESS;
    }

    if ((int32)bot->status.skill_point < total_sp) {
        clif_displaymessage(sd->fd, "Not enough skill points!");
        return SCRIPT_CMD_SUCCESS;
    }

    // 递归学习前置链
    if (!bot_skill_learn_chain(bot, bot_class, skill_id, target_lv, 0)) {
        clif_displaymessage(sd->fd, "Failed to learn skill chain!");
        return SCRIPT_CMD_SUCCESS;
    }

    bot->status.skill_point -= total_sp;
    clif_displaymessage(sd->fd, "Skill learned!");
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_equip)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    int16 idx = (int16)script_getnum(st, 2);

    if (idx < 0 || idx >= MAX_INVENTORY || bot->inventory.u.items_inventory[idx].nameid == 0)
        return SCRIPT_CMD_SUCCESS;

    if (bot->inventory.u.items_inventory[idx].equip) {
        clif_displaymessage(sd->fd, "Item is already equipped!");
        return SCRIPT_CMD_SUCCESS;
    }

    if (!pc_equipitem(bot, idx, pc_equippoint(bot, idx))) {
        clif_displaymessage(sd->fd, "Bot cannot equip this item (level/job requirement?)");
        return SCRIPT_CMD_SUCCESS;
    }
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_unequip)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    int16 idx = (int16)script_getnum(st, 2);

    if (idx < 0 || idx >= MAX_INVENTORY || bot->inventory.u.items_inventory[idx].nameid == 0)
        return SCRIPT_CMD_SUCCESS;
    if (!bot->inventory.u.items_inventory[idx].equip) {
        clif_displaymessage(sd->fd, "Item is not equipped!");
        return SCRIPT_CMD_SUCCESS;
    }

    pc_unequipitem(bot, idx, 0);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_useitem)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    int16 idx = (int16)script_getnum(st, 2);
    if (idx < 0 || idx >= MAX_INVENTORY)
        return SCRIPT_CMD_SUCCESS;
    if (bot->inventory.u.items_inventory[idx].nameid == 0)
        return SCRIPT_CMD_SUCCESS;

    pc_useitem(bot, idx);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_getitem)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    t_itemid item_id = (t_itemid)script_getnum(st, 2);
    int32 amount = script_getnum(st, 3);

    struct item it;
    memset(&it, 0, sizeof(it));
    it.nameid = item_id;
    it.amount = amount;
    it.identify = 1;
    pc_additem(bot, &it, amount, LOG_TYPE_SCRIPT);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_ai_mode)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot)
        return SCRIPT_CMD_SUCCESS;

    int32 mode = script_getnum(st, 2);
    if (mode >= 0 && mode < AI_MODE_MAX)
        sd->bot->ai_mode = (bot_ai_mode)mode;
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_say)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    const char* msg = script_getstr(st, 2);
    clif_displaymessage(sd->fd, msg);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_jobchange)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;
    int32 job_id = script_getnum(st, 2);

    if (bot->status.skill_point > 0) {
        clif_displaymessage(sd->fd, "Use all skill points first!");
        return SCRIPT_CMD_SUCCESS;
    }

    // 保存当前技能
    struct s_skill saved_skill[MAX_SKILL];
    memcpy(saved_skill, bot->status.skill, sizeof(saved_skill));

    // 转职（内部会调 pc_calc_skilltree 清空技能）
    if (!pc_jobchange(bot, job_id, 0)) {
        clif_displaymessage(sd->fd, "Job change failed!");
        return SCRIPT_CMD_SUCCESS;
    }

    // 恢复旧技能（不覆盖新职业树已恢复的技能）
    for (int i = 0; i < MAX_SKILL; i++) {
        if (saved_skill[i].id != 0 && saved_skill[i].lv > 0) {
            if (bot->status.skill[i].id == 0) {
                bot->status.skill[i] = saved_skill[i];
            }
        }
    }

    // 强制重生更新外观
    if (bot->prev != nullptr)
        unit_remove_map(bot, CLR_OUTSIGHT);

    bot->mapindex = sd->mapindex;
    bot->m = sd->m;
    bot->x = sd->x + 1;
    bot->y = sd->y;

    map_addblock(bot);
    clif_spawn(bot);

    clif_displaymessage(sd->fd, "Bot job changed! Skills preserved.");
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_recall)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd)
        return SCRIPT_CMD_SUCCESS;

    map_session_data* bot = sd->bot->bot_sd;

    if (bot->prev != nullptr)
        unit_remove_map(bot, CLR_OUTSIGHT);

    bot->mapindex = sd->mapindex;
    bot->m = sd->m;
    bot->x = sd->x + 1;
    bot->y = sd->y;

    map_addblock(bot);
    clif_spawn(bot);

    clif_displaymessage(sd->fd, "Bot teleported!");
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_getskilllv)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd) {
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }
    script_pushint(st, pc_checkskill(sd->bot->bot_sd, (uint16)script_getnum(st, 2)));
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_revive)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd) {
        clif_displaymessage(sd->fd, "No bot found!");
        return SCRIPT_CMD_SUCCESS;
    }
    map_session_data* bot = sd->bot->bot_sd;
    if (bot->status.hp <= 0) {
        status_revive(bot, 100, 100);
        clif_spawn(bot);
        clif_displaymessage(sd->fd, "Bot revived!");
    } else {
        clif_displaymessage(sd->fd, "Bot is already alive!");
    }
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_giveitem)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd) {
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }
    map_session_data* bot = sd->bot->bot_sd;

    t_itemid nameid = (t_itemid)script_getnum(st, 2);
    int32 amount = script_getnum(st, 3);
    if (amount <= 0) {
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    int16 idx = pc_search_inventory(sd, nameid);
    if (idx < 0 || sd->inventory.u.items_inventory[idx].amount < amount) {
        clif_displaymessage(sd->fd, "Not enough items!");
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    struct item tmp;
    memcpy(&tmp, &sd->inventory.u.items_inventory[idx], sizeof(tmp));
    tmp.amount = amount;

    uint8 add_result = pc_additem(bot, &tmp, amount, LOG_TYPE_SCRIPT);
    if (add_result != ADDITEM_SUCCESS) {
        const char* msg;
        switch (add_result) {
            case ADDITEM_OVERWEIGHT: msg = "Bot can't carry that much weight!"; break;
            case ADDITEM_OVERITEM:   msg = "Bot's inventory is full!"; break;
            case ADDITEM_OVERAMOUNT: msg = "Bot has too many of this item!"; break;
            default:                 msg = "Bot cannot receive the item!";
        }
        clif_displaymessage(sd->fd, msg);
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    pc_delitem(sd, idx, amount, 0, 0, LOG_TYPE_SCRIPT);
    script_pushint(st, 1);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_takeitem)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd) {
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }
    map_session_data* bot = sd->bot->bot_sd;

    t_itemid nameid = (t_itemid)script_getnum(st, 2);
    int32 amount = script_getnum(st, 3);
    if (amount <= 0) {
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    int16 idx = pc_search_inventory(bot, nameid);
    if (idx < 0 || bot->inventory.u.items_inventory[idx].amount < amount) {
        clif_displaymessage(sd->fd, "Bot doesn't have enough items!");
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    struct item tmp;
    memcpy(&tmp, &bot->inventory.u.items_inventory[idx], sizeof(tmp));
    tmp.amount = amount;

    uint8 add_result = pc_additem(sd, &tmp, amount, LOG_TYPE_SCRIPT);
    if (add_result != ADDITEM_SUCCESS) {
        const char* msg;
        switch (add_result) {
            case ADDITEM_OVERWEIGHT: msg = "You can't carry that much weight!"; break;
            case ADDITEM_OVERITEM:   msg = "Your inventory is full!"; break;
            case ADDITEM_OVERAMOUNT: msg = "You have too many of this item!"; break;
            default:                 msg = "Cannot receive the item!";
        }
        clif_displaymessage(sd->fd, msg);
        script_pushint(st, 0);
        return SCRIPT_CMD_SUCCESS;
    }

    pc_delitem(bot, idx, amount, 0, 0, LOG_TYPE_SCRIPT);
    script_pushint(st, 1);
    return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(bot_inventorylist)
{
    map_session_data* sd;
    if (!script_rid2sd(sd) || !sd->bot || !sd->bot->bot_sd) {
        pc_setreg(sd, add_str("@botinv_count"), 0);
        return SCRIPT_CMD_SUCCESS;
    }
    map_session_data* bot = sd->bot->bot_sd;

    int32 j = 0;
    for (int32 i = 0; i < MAX_INVENTORY; i++) {
        if (bot->inventory.u.items_inventory[i].nameid == 0)
            continue;

        pc_setreg(sd, reference_uid(add_str("@botinv_id"), j),
                  bot->inventory.u.items_inventory[i].nameid);
        pc_setreg(sd, reference_uid(add_str("@botinv_idx"), j), i);
        pc_setreg(sd, reference_uid(add_str("@botinv_amount"), j),
                  bot->inventory.u.items_inventory[i].amount);
        pc_setreg(sd, reference_uid(add_str("@botinv_refine"), j),
                  bot->inventory.u.items_inventory[i].refine);
        pc_setreg(sd, reference_uid(add_str("@botinv_identify"), j),
                  bot->inventory.u.items_inventory[i].identify);
        pc_setreg(sd, reference_uid(add_str("@botinv_equip"), j),
                  bot->inventory.u.items_inventory[i].equip);

        std::shared_ptr<item_data> id = item_db.find(bot->inventory.u.items_inventory[i].nameid);
        if (id) {
            pc_setreg(sd, reference_uid(add_str("@botinv_type"), j), id->type);
        } else {
            pc_setreg(sd, reference_uid(add_str("@botinv_type"), j), 0);
        }

        for (int k = 0; k < MAX_SLOTS; k++) {
            char var[32];
            snprintf(var, sizeof(var), "@botinv_card%d", k + 1);
            pc_setreg(sd, reference_uid(add_str(var), j),
                      bot->inventory.u.items_inventory[i].card[k]);
        }
        j++;
    }
    pc_setreg(sd, add_str("@botinv_count"), j);
    return SCRIPT_CMD_SUCCESS;
}
