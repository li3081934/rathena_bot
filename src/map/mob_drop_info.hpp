// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef MOB_DROP_INFO_HPP
#define MOB_DROP_INFO_HPP

#include <common/cbasetypes.hpp>

struct map_session_data;

// Current-map monster + drop-rate viewer (custom packets, 0x0C45-0x0C48
// verified unused by official clients).
// Layouts (all little-endian, same on the roBrowser side):
//   CZ_REQ_MAPMOBS  0x0C45 len 2 : <id>.W
//   ZC_ACK_MAPMOBS  0x0C46 len -1: <id>.W <len>.W <count>.W { <mob id>.W <qty>.W }
//   CZ_REQ_MOBDROPS 0x0C47 len 4 : <id>.W <mob id>.W
//   ZC_ACK_MOBDROPS 0x0C48 len -1: <id>.W <len>.W <mob id>.W <drop count>.W { <item id>.L <rate>.L <flags>.B <type>.B <weight>.L } <mvp count>.W { <item id>.L <rate>.L <type>.B <weight>.L }
// Rates are per-10000 integers already adjusted by mob_getdroprate()
// (server drop multipliers, VIP bonus, caps) for the requesting player.
// flags bit0 = steal_protected.
#define HEADER_CZ_REQ_MAPMOBS  0x0C45
#define HEADER_ZC_ACK_MAPMOBS  0x0C46
#define HEADER_CZ_REQ_MOBDROPS 0x0C47
#define HEADER_ZC_ACK_MOBDROPS 0x0C48

// Incoming packet handlers (registered in clif_packetdb.hpp)
void clif_parse_req_mapmobs(int32 fd, map_session_data *sd);
void clif_parse_req_mobdrops(int32 fd, map_session_data *sd);

#endif /* MOB_DROP_INFO_HPP */
