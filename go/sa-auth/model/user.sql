CREATE TABLE `user` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `display_name` VARCHAR(64) DEFAULT '',
  `is_guest` TINYINT(1) DEFAULT 0,
  `create_time` BIGINT NOT NULL,
  `last_login` BIGINT,
  PRIMARY KEY (`id`),
  INDEX `idx_last_login` (`last_login`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
