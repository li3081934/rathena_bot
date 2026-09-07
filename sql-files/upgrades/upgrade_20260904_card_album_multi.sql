-- Card collection album: multi-activation (one per category, two for accessories)
-- Adds activation ordering and resets existing actives (fresh start per design).
ALTER TABLE `card_collection` ADD COLUMN `activated_at` DATETIME NULL;
UPDATE `card_collection` SET `active` = '0', `activated_at` = NULL;
