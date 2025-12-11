CREATE TABLE `user_password` (
  `user_id` BIGINT UNSIGNED NOT NULL,
  `hash` VARCHAR(128) NOT NULL,
  `create_time` BIGINT NOT NULL,
  PRIMARY KEY (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
