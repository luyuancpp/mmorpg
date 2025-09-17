CREATE TABLE IF NOT EXISTS user_accounts (
	account varchar(256),
	password varchar(256),
	simple_players Blob,
	PRIMARY KEY (account)
) ENGINE = INNODB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS account_share_database (
	account varchar(256),
	PRIMARY KEY (account)
) ENGINE = INNODB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS player_centre_database (
	player_id bigint unsigned NOT NULL AUTO_INCREMENT,
	scene_info Blob,
	PRIMARY KEY (player_id)
) ENGINE = INNODB AUTO_INCREMENT=1 DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS player_database (
	player_id bigint unsigned NOT NULL AUTO_INCREMENT,
	transform Blob,
	uint64_pb_component Blob,
	skill_list Blob,
	uint32_pb_component Blob,
	derived_attributes_component Blob,
	level_component Blob,
	PRIMARY KEY (player_id)
) ENGINE = INNODB AUTO_INCREMENT=1 DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS player_database_1 (
	player_id bigint unsigned NOT NULL AUTO_INCREMENT,
	PRIMARY KEY (player_id)
) ENGINE = INNODB AUTO_INCREMENT=1 DEFAULT CHARSET = utf8mb4;

