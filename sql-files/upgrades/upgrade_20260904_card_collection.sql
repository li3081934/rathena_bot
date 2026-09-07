-- Card collection album (per-character unlocks + active card)
CREATE TABLE IF NOT EXISTS `card_collection` (
  `char_id` INT(11) UNSIGNED NOT NULL,
  `card_id` INT(11) UNSIGNED NOT NULL,
  `active` TINYINT(1) UNSIGNED NOT NULL DEFAULT '0',
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`char_id`, `card_id`),
  KEY `char_active` (`char_id`, `active`)
) ENGINE=InnoDB;
