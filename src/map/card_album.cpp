// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

// Card collection album: submit cards (consumed) to a per-character
// collection and activate card effects through the `bonus_script` system,
// without socketing the cards into equipment.
//
// Activation budget: one card per equip category, two for accessories
// (category 7), i.e. up to 8 active cards. A full category evicts its
// earliest-activated card when a new one is activated.
//
// Persistence: `card_collection(char_id, card_id, active, activated_at)`.
// Each active effect lives in the regular `bonus_script` system with a
// ~10 year duration, so actives survive relogs via the existing char-server
// save/restore (chrif_bsdata_*); entries are tagged per card with
// CARD_ALBUM_MARKER_PREFIX + card id, so per-card removal never touches
// other bonus_scripts.

#include "card_album.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <common/db.hpp>
#include <common/malloc.hpp>
#include <common/nullpo.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/sql.hpp>
#include <common/strlib.hpp>

#include "clif.hpp"
#include "itemdb.hpp"
#include "log.hpp"
#include "map.hpp"
#include "pc.hpp"
#include "status.hpp"

extern Sql *mmysql_handle;

// Marker prefix for album entries. `//` is a valid script comment, so the
// wrapped script parses exactly like the original. The full marker is
// "<prefix>:<card_id>", making each card's entry unique (pc_bonus_script_add
// rejects duplicate script texts) and removable per card.
static const char *CARD_ALBUM_MARKER_PREFIX = "//card_album:";
// Pre-multi-activation marker (no card id). Entries in this legacy format
// are swept on sight so they can't linger as ghost effects after relog.
static const char *CARD_ALBUM_MARKER_LEGACY = "//card_album";
// ~10 years in ms; persists through bonus_script save/restore.
static const t_tick CARD_ALBUM_DURATION = (t_tick)10 * 365 * 24 * 3600 * 1000;
static const char *card_collection_table = "card_collection";

// Max simultaneously active cards per category (2 for accessories).
static uint8 card_album_category_limit(uint8 category) {
	return (category == 7) ? 2 : 1;
}

static std::string card_album_marker(t_itemid card_id) {
	char buf[32];

	snprintf(buf, sizeof(buf), "%s%u", CARD_ALBUM_MARKER_PREFIX, (uint32)card_id);
	return std::string(buf);
}

/// If `script` is an album entry, extracts its card id. Returns true for
/// per-card entries, false otherwise (legacy entries report card id 0).
static bool card_album_parse_marker(const char *script, t_itemid *card_id, bool *legacy) {
	size_t prefix_len = strlen(CARD_ALBUM_MARKER_PREFIX);

	if (!script || strncmp(script, CARD_ALBUM_MARKER_PREFIX, prefix_len) != 0) {
		if (script && strncmp(script, CARD_ALBUM_MARKER_LEGACY, strlen(CARD_ALBUM_MARKER_LEGACY)) == 0 &&
			(script[strlen(CARD_ALBUM_MARKER_LEGACY)] == '\n' || script[strlen(CARD_ALBUM_MARKER_LEGACY)] == '\0')) {
			if (card_id)
				*card_id = 0;
			if (legacy)
				*legacy = true;
			return false;
		}
		return false;
	}
	if (card_id)
		*card_id = (t_itemid)strtoul(script + prefix_len, nullptr, 10);
	if (legacy)
		*legacy = false;
	return true;
}

static bool card_album_unlocked(int32 char_id, t_itemid card_id) {
	if (SQL_ERROR == Sql_Query(mmysql_handle, "SELECT `card_id` FROM `%s` WHERE `char_id` = '%d' AND `card_id` = '%u' LIMIT 1",
		card_collection_table, char_id, card_id)) {
		Sql_ShowDebug(mmysql_handle);
		return false;
	}
	bool found = (Sql_NumRows(mmysql_handle) > 0);
	Sql_FreeResult(mmysql_handle);
	return found;
}

static std::vector<t_itemid> card_album_active_ids(int32 char_id) {
	std::vector<t_itemid> actives;
	char *data;

	if (SQL_ERROR == Sql_Query(mmysql_handle, "SELECT `card_id` FROM `%s` WHERE `char_id` = '%d' AND `active` = '1' ORDER BY `card_id`",
		card_collection_table, char_id)) {
		Sql_ShowDebug(mmysql_handle);
		return actives;
	}
	while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
		Sql_GetData(mmysql_handle, 0, &data, nullptr);
		actives.push_back((t_itemid)strtoul(data, nullptr, 10));
	}
	Sql_FreeResult(mmysql_handle);
	return actives;
}

/// Remove one card's album bonus_script entry. Returns true when removed.
static bool card_album_remove_effect(map_session_data *sd, t_itemid card_id) {
	struct linkdb_node *node = nullptr, *next = nullptr;
	bool removed = false;

	if (!sd || card_id == 0 || !(node = sd->bonus_script.head))
		return false;

	while (node) {
		next = node->next;
		struct s_bonus_script_entry *entry = (struct s_bonus_script_entry *)node->data;

		if (entry && entry->script_buf) {
			t_itemid entry_id = 0;

			if (card_album_parse_marker(StringBuf_Value(entry->script_buf), &entry_id, nullptr) && entry_id == card_id) {
				linkdb_erase(&sd->bonus_script.head, (void *)((intptr_t)entry));
				pc_bonus_script_free_entry(sd, entry);
				removed = true;
			}
		}
		node = next;
	}

	if (!removed)
		return false;

	if (sd->bonus_script.count == 0) {
		if (sd->bonus_script.head && sd->bonus_script.head->data)
			pc_bonus_script_free_entry(sd, (struct s_bonus_script_entry *)sd->bonus_script.head->data);
		linkdb_final(&sd->bonus_script.head);
	}
	status_calc_pc(sd, SCO_NONE);
	return true;
}

/// Remove legacy-format album entries (marker without card id). Returns true
/// when anything was removed.
static bool card_album_sweep_legacy(map_session_data *sd) {
	struct linkdb_node *node = nullptr, *next = nullptr;
	bool removed = false;

	if (!sd || !(node = sd->bonus_script.head))
		return false;

	while (node) {
		next = node->next;
		struct s_bonus_script_entry *entry = (struct s_bonus_script_entry *)node->data;

		if (entry && entry->script_buf) {
			t_itemid entry_id = 0;
			bool legacy = false;

			if (!card_album_parse_marker(StringBuf_Value(entry->script_buf), &entry_id, &legacy) && legacy) {
				linkdb_erase(&sd->bonus_script.head, (void *)((intptr_t)entry));
				pc_bonus_script_free_entry(sd, entry);
				removed = true;
			}
		}
		node = next;
	}

	if (!removed)
		return false;

	if (sd->bonus_script.count == 0) {
		if (sd->bonus_script.head && sd->bonus_script.head->data)
			pc_bonus_script_free_entry(sd, (struct s_bonus_script_entry *)sd->bonus_script.head->data);
		linkdb_final(&sd->bonus_script.head);
	}
	status_calc_pc(sd, SCO_NONE);
	return true;
}

/// Apply a card's script as a bonus_script entry. Returns false on parse failure.
static bool card_album_apply_effect(map_session_data *sd, t_itemid card_id) {
	const char *source = itemdb_get_script_source(card_id);

	if (!source || !source[0])
		return false;

	std::string wrapped = card_album_marker(card_id) + "\n" + source;
	struct s_bonus_script_entry *entry = pc_bonus_script_add(sd, wrapped.c_str(), CARD_ALBUM_DURATION, EFST_BLANK, BSF_PERMANENT, 0);

	if (!entry)
		return false;
	linkdb_insert(&sd->bonus_script.head, (void *)((intptr_t)entry), entry);
	status_calc_pc(sd, SCO_NONE);
	return true;
}

// Count distinct equip groups in a bitmask (hand_r, hand_l, armor,
// garment, shoes, helm, accessory).
static int32 card_album_equip_groups(uint32 equip) {
	int32 groups = 0;

	if (equip & EQP_HAND_R)
		groups++;
	if (equip & EQP_HAND_L)
		groups++;
	if (equip & EQP_ARMOR)
		groups++;
	if (equip & EQP_GARMENT)
		groups++;
	if (equip & EQP_SHOES)
		groups++;
	if (equip & EQP_HELM)
		groups++;
	if (equip & EQP_ACC)
		groups++;
	return groups;
}

// Map a card's equip bitmask to a collection category:
// 1 weapon (right hand), 2 shield (left hand), 3 armor, 4 garment,
// 5 shoes, 6 headgear, 7 accessory, 8 other (no location).
static uint8 card_album_category(uint32 equip) {
	if (equip == 0)
		return 8;
	if (equip & EQP_HAND_R)
		return 1;
	if (equip & EQP_HAND_L)
		return 2;
	if (equip & EQP_ARMOR)
		return 3;
	if (equip & EQP_GARMENT)
		return 4;
	if (equip & EQP_SHOES)
		return 5;
	if (equip & EQP_HELM)
		return 6;
	if (equip & EQP_ACC)
		return 7;
	return 8;
}

/// Returns the collection category of a card id, or 8 when unknown.
static uint8 card_album_category_of(t_itemid card_id) {
	std::shared_ptr<item_data> data = item_db.find(card_id);

	if (!data)
		return 8;
	return card_album_category(data->equip);
}

/// Reconcile runtime bonus entries with the DB active set: sweep legacy
/// entries, drop entries whose card is no longer active, and re-apply
/// missing actives (covers relog drift).
static void card_album_reconcile(map_session_data *sd) {
	std::vector<t_itemid> actives;
	struct linkdb_node *node = nullptr, *next = nullptr;
	bool changed = false;

	if (!sd)
		return;

	if (card_album_sweep_legacy(sd))
		changed = true;

	actives = card_album_active_ids(sd->status.char_id);

	// Drop entries for cards that are not DB-active
	if ((node = sd->bonus_script.head)) {
		while (node) {
			next = node->next;
			struct s_bonus_script_entry *entry = (struct s_bonus_script_entry *)node->data;

			if (entry && entry->script_buf) {
				t_itemid entry_id = 0;

				if (card_album_parse_marker(StringBuf_Value(entry->script_buf), &entry_id, nullptr) && entry_id != 0 &&
					std::find(actives.begin(), actives.end(), entry_id) == actives.end()) {
					linkdb_erase(&sd->bonus_script.head, (void *)((intptr_t)entry));
					pc_bonus_script_free_entry(sd, entry);
					changed = true;
				}
			}
			node = next;
		}
	}

	// Re-apply DB-actives missing at runtime
	for (t_itemid card_id : actives) {
		bool present = false;

		for (node = sd->bonus_script.head; node; node = node->next) {
			struct s_bonus_script_entry *entry = (struct s_bonus_script_entry *)node->data;

			if (entry && entry->script_buf) {
				t_itemid entry_id = 0;

				if (card_album_parse_marker(StringBuf_Value(entry->script_buf), &entry_id, nullptr) && entry_id == card_id) {
					present = true;
					break;
				}
			}
		}
		if (!present && card_album_apply_effect(sd, card_id))
			changed = true;
	}

	if (changed) {
		if (sd->bonus_script.count == 0) {
			if (sd->bonus_script.head && sd->bonus_script.head->data)
				pc_bonus_script_free_entry(sd, (struct s_bonus_script_entry *)sd->bonus_script.head->data);
			linkdb_final(&sd->bonus_script.head);
		}
		status_calc_pc(sd, SCO_NONE);
	}
}

void clif_card_album_list(map_session_data *sd) {
	nullpo_retv(sd);

	int32 fd = sd->fd;
	std::vector<t_itemid> unlocked;
	std::vector<std::pair<t_itemid, uint8>> catalog;
	char *data;

	card_album_reconcile(sd);

	std::vector<t_itemid> actives = card_album_active_ids(sd->status.char_id);

	if (SQL_ERROR == Sql_Query(mmysql_handle, "SELECT `card_id` FROM `%s` WHERE `char_id` = '%d' ORDER BY `card_id`",
		card_collection_table, sd->status.char_id)) {
		Sql_ShowDebug(mmysql_handle);
		return;
	}

	while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
		Sql_GetData(mmysql_handle, 0, &data, nullptr);
		unlocked.push_back((t_itemid)strtoul(data, nullptr, 10));
	}
	Sql_FreeResult(mmysql_handle);

	// Full card catalog from the item database: monster cards only
	// (CARD_NORMAL with at least one equip location spanning at most 2
	// equip groups; location-less stones, enchants and all-location
	// tarot/event cards excluded), with per-card equip category.
	for (auto it = item_db.begin(); it != item_db.end(); ++it) {
		if (it->second && it->second->type == IT_CARD && it->second->subtype == CARD_NORMAL && it->second->equip != 0 &&
			card_album_equip_groups(it->second->equip) <= 2)
			catalog.push_back({ it->first, card_album_category(it->second->equip) });
	}
	std::sort(catalog.begin(), catalog.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

	uint16 active_count = (uint16)actives.size();
	uint16 unlock_count = (uint16)unlocked.size();
	uint16 catalog_count = (uint16)catalog.size();
	int32 len = 8 + active_count * 4 + 2 + unlock_count * 4 + 2 + catalog_count * 5;

	WFIFOHEAD(fd, len);
	WFIFOW(fd, 0) = HEADER_ZC_CARD_ALBUM_LIST;
	WFIFOW(fd, 2) = len;
	WFIFOW(fd, 4) = active_count;

	int32 pos = 6;
	for (uint16 i = 0; i < active_count; i++, pos += 4)
		WFIFOL(fd, pos) = (uint32)actives[i];

	WFIFOW(fd, pos) = unlock_count;
	pos += 2;
	for (uint16 i = 0; i < unlock_count; i++, pos += 4)
		WFIFOL(fd, pos) = (uint32)unlocked[i];

	WFIFOW(fd, pos) = catalog_count;
	pos += 2;
	for (uint16 i = 0; i < catalog_count; i++, pos += 5) {
		WFIFOL(fd, pos) = (uint32)catalog[i].first;
		WFIFOB(fd, pos + 4) = catalog[i].second;
	}

	WFIFOSET(fd, len);
}

static void clif_card_album_ack(map_session_data *sd, uint8 action, uint8 result, t_itemid card_id) {
	int32 fd = sd->fd;

	WFIFOHEAD(fd, 8);
	WFIFOW(fd, 0) = HEADER_ZC_CARD_ALBUM_ACK;
	WFIFOB(fd, 2) = action;
	WFIFOB(fd, 3) = result;
	WFIFOL(fd, 4) = card_id;
	WFIFOSET(fd, 8);
}

void clif_parse_card_album_list(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);
	clif_card_album_list(sd);
}

void clif_parse_card_album_submit(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);

	// Client sends client-space indices (server index + 2), same as USE_ITEM
	uint16 raw_idx = (uint16)RFIFOW(fd, 2);

	if (raw_idx < 2) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_ERR_INVALID, 0);
		return;
	}

	int16 idx = (int16)(raw_idx - 2);

	if (pc_isdead(sd) || idx < 0 || idx >= MAX_INVENTORY ||
		sd->inventory.u.items_inventory[idx].nameid == 0 || sd->inventory_data[idx] == nullptr) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_ERR_INVALID, 0);
		return;
	}

	t_itemid nameid = sd->inventory.u.items_inventory[idx].nameid;
	std::shared_ptr<item_data> data = item_db.find(nameid);

	// Monster cards only: plain IT_CARD with CARD_NORMAL subtype, at least
	// one equip location and at most 2 equip groups (location-less stones,
	// enchants and all-location tarot/event cards rejected)
	if (!data || data->type != IT_CARD || data->subtype != CARD_NORMAL || data->equip == 0 ||
		card_album_equip_groups(data->equip) > 2) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_ERR_NOT_CARD, nameid);
		return;
	}

	const char *source = itemdb_get_script_source(nameid);

	if (!source || !source[0]) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_ERR_NO_SCRIPT, nameid);
		return;
	}

	if (pc_delitem(sd, idx, 1, 0, 0, LOG_TYPE_CONSUME)) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_ERR_INVALID, nameid);
		return;
	}

	if (SQL_ERROR == Sql_Query(mmysql_handle,
		"INSERT IGNORE INTO `%s` (`char_id`,`card_id`,`active`) VALUES ('%d','%u','0')",
		card_collection_table, sd->status.char_id, nameid)) {
		Sql_ShowDebug(mmysql_handle);
		clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_ERR_DB, nameid);
		return;
	}

	clif_card_album_ack(sd, CARD_ALBUM_ACT_SUBMIT, CARD_ALBUM_OK, nameid);
	clif_card_album_list(sd);
}

void clif_parse_card_album_activate(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);

	t_itemid card_id = (t_itemid)RFIFOL(fd, 2);
	char *data;

	if (pc_isdead(sd) || card_id == 0) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_INVALID, card_id);
		return;
	}

	if (!card_album_unlocked(sd->status.char_id, card_id)) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_NOT_UNLOCKED, card_id);
		return;
	}

	uint8 category = card_album_category_of(card_id);
	uint8 limit = card_album_category_limit(category);

	// Already active: refresh duration and recency, nothing else changes.
	std::vector<t_itemid> actives = card_album_active_ids(sd->status.char_id);

	if (std::find(actives.begin(), actives.end(), card_id) != actives.end()) {
		card_album_remove_effect(sd, card_id);
		if (!card_album_apply_effect(sd, card_id)) {
			clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_NO_SCRIPT, card_id);
			return;
		}
		if (SQL_ERROR == Sql_Query(mmysql_handle, "UPDATE `%s` SET `activated_at` = NOW() WHERE `char_id` = '%d' AND `card_id` = '%u'",
			card_collection_table, sd->status.char_id, card_id)) {
			Sql_ShowDebug(mmysql_handle);
			clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_DB, card_id);
			return;
		}
		clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_OK, card_id);
		clif_card_album_list(sd);
		return;
	}

	// Collect actives in the same category, earliest first.
	std::vector<t_itemid> same_cat;

	if (SQL_ERROR == Sql_Query(mmysql_handle,
		"SELECT `card_id` FROM `%s` WHERE `char_id` = '%d' AND `active` = '1' ORDER BY `activated_at` ASC, `card_id` ASC",
		card_collection_table, sd->status.char_id)) {
		Sql_ShowDebug(mmysql_handle);
		clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_DB, card_id);
		return;
	}
	while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
		Sql_GetData(mmysql_handle, 0, &data, nullptr);
		t_itemid other = (t_itemid)strtoul(data, nullptr, 10);

		if (card_album_category_of(other) == category)
			same_cat.push_back(other);
	}
	Sql_FreeResult(mmysql_handle);

	// Apply first so a parse failure leaves the current set untouched.
	if (!card_album_apply_effect(sd, card_id)) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_NO_SCRIPT, card_id);
		return;
	}

	bool replaced = false;

	if (same_cat.size() >= limit) {
		t_itemid victim = same_cat.front();

		card_album_remove_effect(sd, victim);
		if (SQL_ERROR == Sql_Query(mmysql_handle, "UPDATE `%s` SET `active` = '0', `activated_at` = NULL WHERE `char_id` = '%d' AND `card_id` = '%u'",
			card_collection_table, sd->status.char_id, victim)) {
			Sql_ShowDebug(mmysql_handle);
			card_album_remove_effect(sd, card_id);
			clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_DB, card_id);
			return;
		}
		replaced = true;
	}

	if (SQL_ERROR == Sql_Query(mmysql_handle, "UPDATE `%s` SET `active` = '1', `activated_at` = NOW() WHERE `char_id` = '%d' AND `card_id` = '%u'",
		card_collection_table, sd->status.char_id, card_id)) {
		Sql_ShowDebug(mmysql_handle);
		clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, CARD_ALBUM_ERR_DB, card_id);
		return;
	}

	clif_card_album_ack(sd, CARD_ALBUM_ACT_ACTIVATE, replaced ? CARD_ALBUM_OK_REPLACED : CARD_ALBUM_OK, card_id);
	clif_card_album_list(sd);
}

void clif_parse_card_album_deactivate(int32 fd, map_session_data *sd) {
	nullpo_retv(sd);

	t_itemid card_id = (t_itemid)RFIFOL(fd, 2);

	if (pc_isdead(sd) || card_id == 0) {
		clif_card_album_ack(sd, CARD_ALBUM_ACT_DEACTIVATE, CARD_ALBUM_ERR_INVALID, card_id);
		return;
	}

	card_album_remove_effect(sd, card_id);

	if (SQL_ERROR == Sql_Query(mmysql_handle, "UPDATE `%s` SET `active` = '0', `activated_at` = NULL WHERE `char_id` = '%d' AND `card_id` = '%u'",
		card_collection_table, sd->status.char_id, card_id)) {
		Sql_ShowDebug(mmysql_handle);
		clif_card_album_ack(sd, CARD_ALBUM_ACT_DEACTIVATE, CARD_ALBUM_ERR_DB, card_id);
		return;
	}

	clif_card_album_ack(sd, CARD_ALBUM_ACT_DEACTIVATE, CARD_ALBUM_OK, card_id);
	clif_card_album_list(sd);
}
