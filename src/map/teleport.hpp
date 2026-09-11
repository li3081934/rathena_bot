// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef TELEPORT_HPP
#define TELEPORT_HPP

#include <common/cbasetypes.hpp>

// Convenience teleport (custom packets; 0x0C49/0x0C4A are unused officially).
//
//   CZ_REQ_TELEPORT  0x0C49 len 22 : <id>.W <map>.16B <x>.W <y>.W
//   ZC_ACK_TELEPORT  0x0C4A len  6 : <id>.W <result>.B <pad>.B <param>.W
//
// x = y = 0 asks the server to pick a random walkable cell.

#define HEADER_CZ_REQ_TELEPORT 0x0C49
#define HEADER_ZC_ACK_TELEPORT 0x0C4A

enum e_teleport_result : uint8 {
	TELEPORT_OK = 0,
	TELEPORT_ERR_NOT_FOUND = 1,    // map does not exist
	TELEPORT_ERR_BLOCKED = 2,      // map flags forbid warp to/from
	TELEPORT_ERR_DENIED = 3,       // no permission
	TELEPORT_ERR_MISSING_ITEM = 4, // required item not owned
	TELEPORT_ERR_COOLDOWN = 5,     // still on cooldown
	TELEPORT_ERR_BAD_COORDS = 6,   // invalid coordinates
	TELEPORT_ERR_JOB = 7           // job / group level gate
};

struct map_session_data;

void clif_parse_req_teleport(int32 fd, map_session_data *sd);

#endif /* TELEPORT_HPP */
