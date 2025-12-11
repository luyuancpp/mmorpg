CREATE TABLE `user_phone` (
  `user_id` BIGINT UNSIGNED NOT NULL,
  `phone` VARCHAR(32) NOT NULL,
  `create_time` BIGINT NOT NULL,
  PRIMARY KEY (`user_id`),
  UNIQUE KEY `uk_phone` (`phone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
