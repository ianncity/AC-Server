-- The cache is derived data. Discard possible partial MyISAM writes before rebuilding transactionally.
ALTER TABLE `playerbots_rnditem_cache` ENGINE=InnoDB ROW_FORMAT=DYNAMIC;
DELETE FROM `playerbots_rnditem_cache`;
