CREATE TABLE IF NOT EXISTS `playerbots_advai_market_sales` (
  `house` INT UNSIGNED NOT NULL,
  `item` INT UNSIGNED NOT NULL,
  `hour` BIGINT UNSIGNED NOT NULL,
  `units` BIGINT UNSIGNED NOT NULL,
  `revenue` BIGINT UNSIGNED NOT NULL,
  `last_sale` BIGINT UNSIGNED NOT NULL,
  PRIMARY KEY (`house`, `item`, `hour`),
  KEY `expiry` (`hour`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
