// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef CARD_ALBUM_HPP
#define CARD_ALBUM_HPP

#include <common/cbasetypes.hpp>

struct map_session_data;

// Custom packets (0x0B46-0x0B4B are unused by official clients).
// Layouts (all little-endian, same on the roBrowser side):
//   CZ_CARD_ALBUM_LIST_REQ  0x0B46 len 2 : <id>.W
//   CZ_CARD_ALBUM_SUBMIT    0x0B47 len 4 : <id>.W <inv index>.W
//   CZ_CARD_ALBUM_ACTIVATE  0x0B48 len 6 : <id>.W <card id>.L
//   CZ_CARD_ALBUM_DEACTIVATE 0x0B49 len 6 : <id>.W <card id>.L
//   ZC_CARD_ALBUM_LIST      0x0B4A len -1: <id>.W <len>.W <active count>.W { <card id>.L } <unlock count>.W { <card id>.L } <catalog count>.W { <card id>.L <category>.B }
//   ZC_CARD_ALBUM_ACK       0x0B4B len 8 : <id>.W <action>.B <result>.B <card id>.L
#define HEADER_CZ_CARD_ALBUM_LIST_REQ  0x0B46
#define HEADER_CZ_CARD_ALBUM_SUBMIT    0x0B47
#define HEADER_CZ_CARD_ALBUM_ACTIVATE  0x0B48
#define HEADER_CZ_CARD_ALBUM_DEACTIVATE 0x0B49
// Card catalog categories (per-card equip location):
// 1 weapon, 2 shield, 3 armor, 4 garment, 5 shoes, 6 headgear, 7 accessory.
// 8 is reserved as client-side fallback and never sent.
// Only CARD_NORMAL monster cards with 1-2 equip groups are listed or
// accepted for submit (enchants, location-less stones and all-location
// tarot/event cards out).
#define HEADER_ZC_CARD_ALBUM_LIST      0x0B4A
#define HEADER_ZC_CARD_ALBUM_ACK       0x0B4B

// ACK actions
enum e_card_album_action : uint8 {
	CARD_ALBUM_ACT_SUBMIT = 1,
	CARD_ALBUM_ACT_ACTIVATE = 2,
	CARD_ALBUM_ACT_DEACTIVATE = 3,
};

// ACK results
enum e_card_album_result : uint8 {
	CARD_ALBUM_OK = 0,
	CARD_ALBUM_ERR_INVALID = 1,   // bad index / dead / wrong state
	CARD_ALBUM_ERR_NOT_CARD = 2,  // item is not a card
	CARD_ALBUM_ERR_NO_SCRIPT = 3, // card has no usable script
	CARD_ALBUM_ERR_NOT_UNLOCKED = 4,
	CARD_ALBUM_ERR_DB = 5,
	CARD_ALBUM_OK_REPLACED = 6,   // activate OK, evicted earliest card in category
};

// Incoming packet handlers (registered in clif_packetdb.hpp)
void clif_parse_card_album_list(int32 fd, map_session_data *sd);
void clif_parse_card_album_submit(int32 fd, map_session_data *sd);
void clif_parse_card_album_activate(int32 fd, map_session_data *sd);
void clif_parse_card_album_deactivate(int32 fd, map_session_data *sd);

#endif /* CARD_ALBUM_HPP */
