// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

// Convenience teleport: server-authoritative warp to a requested map.
//
// This mirrors the @mapmove guards (map lookup, NOWARPTO/NOWARP, job gate,
// random free cell when x/y are 0) but is reachable by normal players. All
// progression checks (item cost, cooldown, permission) are meant to live in
// teleport_can_use() below so the client stays a dumb requester.

#include "teleport.hpp"

#include <common/mapindex.hpp>
#include <common/nullpo.hpp>
#include <common/socket.hpp>
#include <common/strlib.hpp>

#include "clif.hpp"
#include "map.hpp"
#include "packets.hpp"
#include "pc.hpp"

// ---------------------------------------------------------------------------
// Permission / cost gate.
//
// Currently open to everyone. To charge for teleports, enable the item block
// below (and include "log.hpp" for LOG_TYPE_OTHER):
//
//   static const t_itemid TELEPORT_ITEM_ID = 12522; // Fly Wing, for example
//   int16 idx = pc_search_inventory(sd, TELEPORT_ITEM_ID);
//   if (idx < 0) { *result = TELEPORT_ERR_MISSING_ITEM; *param = (uint16)TELEPORT_ITEM_ID; return false; }
//   pc_delitem(sd, idx, 1, 0, LOG_TYPE_OTHER);
//
// Cooldowns can be tracked on a per-account timestamp and reported with
// TELEPORT_ERR_COOLDOWN + remaining seconds in *param.
// ---------------------------------------------------------------------------
static bool teleport_can_use(map_session_data *sd, uint8 *result, uint16 *param) {
	(void)sd;
	(void)result;
	(void)param;
	return true;
}

static void clif_ack_teleport(map_session_data *sd, uint8 result, uint16 param) {
	int32 fd = sd->fd;

	WFIFOHEAD(fd, 6);
	WFIFOW(fd, 0) = HEADER_ZC_ACK_TELEPORT;
	WFIFOB(fd, 2) = result;
	WFIFOB(fd, 3) = 0; // reserved
	WFIFOW(fd, 4) = param;
	WFIFOSET(fd, 6);
}

void clif_parse_req_teleport(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);

	// Same wire layout as CZ_MOVETO_MAP: <map>.16B <x>.W <y>.W.
	const PACKET_CZ_MOVETO_MAP *p = reinterpret_cast<PACKET_CZ_MOVETO_MAP *>(RFIFOP(fd, 0));

	char map_name[MAP_NAME_LENGTH_EXT];
	safestrncpy(map_name, p->map, sizeof(map_name));

	int16 x = static_cast<int16>(p->x);
	int16 y = static_cast<int16>(p->y);

	uint16 mapindex = mapindex_name2idx(map_name, nullptr);
	int16 m = mapindex ? map_mapindex2mapid(mapindex) : -1;

	if (m < 0) {
		clif_ack_teleport(sd, TELEPORT_ERR_NOT_FOUND, 0);
		return;
	}

	uint8 fail_result = 0;
	uint16 fail_param = 0;
	if (!teleport_can_use(sd, &fail_result, &fail_param)) {
		clif_ack_teleport(sd, fail_result, fail_param);
		return;
	}

	// Out-of-bounds / impassable explicit coords fall back to a random cell.
	if ((x || y) && map_getcell(m, x, y, CELL_CHKNOPASS)) {
		if (!map_search_freecell(nullptr, m, &x, &y, 10, 10, 1)) {
			x = y = 0;
		}
	}

	if ((map_getmapflag(m, MF_NOWARPTO) && !pc_has_permission(sd, PC_PERM_WARP_ANYWHERE)) ||
	    !pc_job_can_entermap(static_cast<e_job>(sd->status.class_), m, pc_get_group_level(sd))) {
		clif_ack_teleport(sd, TELEPORT_ERR_JOB, 0);
		return;
	}

	if (sd->m >= 0 && map_getmapflag(sd->m, MF_NOWARP) && !pc_has_permission(sd, PC_PERM_WARP_ANYWHERE)) {
		clif_ack_teleport(sd, TELEPORT_ERR_BLOCKED, 0);
		return;
	}

	if (pc_setpos(sd, mapindex, x, y, CLR_TELEPORT) != SETPOS_OK) {
		clif_ack_teleport(sd, TELEPORT_ERR_NOT_FOUND, 0);
		return;
	}

	clif_ack_teleport(sd, TELEPORT_OK, 0);
}
