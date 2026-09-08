// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

// Current-map monster + drop-rate viewer: read-only queries against the
// map server's in-memory mob_db / map moblist, answered with custom
// packets. Same drop math as @mobinfo (mob_getdroprate + MVP递减), so the
// rates shown always match the server's effective configuration.

#include "mob_drop_info.hpp"

#include <vector>

#include <common/nullpo.hpp>
#include <common/socket.hpp>

#include "battle.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "pc.hpp"

struct s_drop_entry {
	t_itemid nameid;
	uint32 rate; // per-10000, already adjusted for the requesting player
	uint8 flags; // bit0 = steal_protected
	uint8 type; // item_types enum (matches roBrowser ItemType ints 1:1)
	uint32 weight; // raw item weight (divide by 10 for display)
};

static void clif_ack_mapmobs_empty(int32 fd) {
	WFIFOHEAD(fd, 6);
	WFIFOW(fd, 0) = HEADER_ZC_ACK_MAPMOBS;
	WFIFOW(fd, 2) = 6;
	WFIFOW(fd, 4) = 0;
	WFIFOSET(fd, 6);
}

static void clif_ack_mobdrops_empty(int32 fd, uint16 mob_id) {
	WFIFOHEAD(fd, 10);
	WFIFOW(fd, 0) = HEADER_ZC_ACK_MOBDROPS;
	WFIFOW(fd, 2) = 10;
	WFIFOW(fd, 4) = mob_id;
	WFIFOW(fd, 6) = 0;
	WFIFOW(fd, 8) = 0;
	WFIFOSET(fd, 10);
}

void clif_parse_req_mapmobs(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);

	struct map_data *md = map_getmapdata(sd->m);
	if (md == nullptr) {
		clif_ack_mapmobs_empty(fd);
		return;
	}

	// Aggregate spawn_data entries by mob id (a map usually has several
	// spawn entries per monster).
	uint16 ids[MAX_MOB_LIST_PER_MAP];
	uint16 qtys[MAX_MOB_LIST_PER_MAP];
	uint16 count = 0;

	for (int32 i = 0; i < MAX_MOB_LIST_PER_MAP; i++) {
		struct spawn_data *spawn = md->moblist[i];
		if (spawn == nullptr || spawn->id <= 0 || !mobdb_checkid(spawn->id))
			continue;
		uint16 id = (uint16)spawn->id;
		int32 j;
		for (j = 0; j < count; j++) {
			if (ids[j] == id)
				break;
		}
		if (j < count) {
			qtys[j] += spawn->num;
		} else {
			ids[count] = id;
			qtys[count] = spawn->num;
			count++;
		}
	}

	int32 len = 6 + count * 4;
	WFIFOHEAD(fd, len);
	WFIFOW(fd, 0) = HEADER_ZC_ACK_MAPMOBS;
	WFIFOW(fd, 2) = len;
	WFIFOW(fd, 4) = count;
	int32 pos = 6;
	for (uint16 i = 0; i < count; i++, pos += 4) {
		WFIFOW(fd, pos) = ids[i];
		WFIFOW(fd, pos + 2) = qtys[i];
	}
	WFIFOSET(fd, len);
}

void clif_parse_req_mobdrops(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);

	uint16 mob_id = (uint16)RFIFOW(fd, 2);
	if (!mobdb_checkid(mob_id)) {
		clif_ack_mobdrops_empty(fd, mob_id);
		return;
	}

	std::shared_ptr<s_mob_db> mob = mob_db.find(mob_id);
	if (mob == nullptr) {
		clif_ack_mobdrops_empty(fd, mob_id);
		return;
	}

	std::vector<s_drop_entry> drops;
	std::vector<s_drop_entry> mvps;
	int32 drop_modifier = 100;

	for (const std::shared_ptr<s_mob_drop>& entry : mob->dropitem) {
		if (entry->nameid == 0 || entry->rate < 1)
			continue;
		if (!item_db.exists(entry->nameid))
			continue;
		int32 rate = mob_getdroprate(sd, mob, entry->rate, drop_modifier);
		if (rate < 0)
			rate = 0;
		s_drop_entry out;
		out.nameid = entry->nameid;
		out.rate = (uint32)rate;
		out.flags = entry->steal_protected ? 1 : 0;
		std::shared_ptr<item_data> idata = item_db.find(entry->nameid);
		out.type = idata != nullptr ? (uint8)idata->type : (uint8)IT_UNKNOWN;
		out.weight = idata != nullptr ? idata->weight : 0;
		drops.push_back(out);
	}

	if (mob->get_bosstype() == BOSSTYPE_MVP) {
		// Same diminishing formula as @mobinfo: with 3 MVP drops at 50%,
		// the first has 50%, the second 25%, the third 12.5%.
		float mvpremain = 100.0f;
		for (const std::shared_ptr<s_mob_drop>& entry : mob->mvpitem) {
			if (entry->nameid == 0)
				continue;
			if (!item_db.exists(entry->nameid))
				continue;
			float mvppercent = (float)entry->rate * mvpremain / 10000.0f;
			if (battle_config.item_drop_mvp_mode == 0)
				mvpremain -= mvppercent;
			if (mvppercent > 0) {
				s_drop_entry out;
				out.nameid = entry->nameid;
				out.rate = (uint32)(mvppercent * 100);
				out.flags = 0;
				std::shared_ptr<item_data> idata = item_db.find(entry->nameid);
				out.type = idata != nullptr ? (uint8)idata->type : (uint8)IT_UNKNOWN;
				out.weight = idata != nullptr ? idata->weight : 0;
				mvps.push_back(out);
			}
		}
	}

	uint16 drop_count = (uint16)drops.size();
	uint16 mvp_count = (uint16)mvps.size();
	int32 len = 10 + drop_count * 14 + 2 + mvp_count * 13;
	WFIFOHEAD(fd, len);
	WFIFOW(fd, 0) = HEADER_ZC_ACK_MOBDROPS;
	WFIFOW(fd, 2) = len;
	WFIFOW(fd, 4) = mob_id;
	WFIFOW(fd, 6) = drop_count;
	int32 pos = 8;
	for (uint16 i = 0; i < drop_count; i++, pos += 14) {
		WFIFOL(fd, pos) = (uint32)drops[i].nameid;
		WFIFOL(fd, pos + 4) = drops[i].rate;
		WFIFOB(fd, pos + 8) = drops[i].flags;
		WFIFOB(fd, pos + 9) = drops[i].type;
		WFIFOL(fd, pos + 10) = drops[i].weight;
	}
	WFIFOW(fd, pos) = mvp_count;
	pos += 2;
	for (uint16 i = 0; i < mvp_count; i++, pos += 13) {
		WFIFOL(fd, pos) = (uint32)mvps[i].nameid;
		WFIFOL(fd, pos + 4) = mvps[i].rate;
		WFIFOB(fd, pos + 8) = mvps[i].type;
		WFIFOL(fd, pos + 9) = mvps[i].weight;
	}
	WFIFOSET(fd, len);
}
