-- Guild & Friend service tables
-- Add to the main database or run separately for the guild/friend schema.

-- ── Guild ────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS guild (
  guild_id       BIGINT UNSIGNED NOT NULL,
  name           VARCHAR(64)     NOT NULL,
  leader_id      BIGINT UNSIGNED NOT NULL DEFAULT 0,
  level          INT UNSIGNED    NOT NULL DEFAULT 1,
  announcement   TEXT,
  create_time_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
  max_members    INT UNSIGNED    NOT NULL DEFAULT 50,
  PRIMARY KEY (guild_id),
  UNIQUE KEY uk_name (name),
  KEY idx_leader (leader_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='guild';

CREATE TABLE IF NOT EXISTS guild_member (
  guild_id    BIGINT UNSIGNED NOT NULL,
  player_id   BIGINT UNSIGNED NOT NULL,
  role        TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=member,2=officer,3=leader',
  join_time_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (guild_id, player_id),
  KEY idx_player (player_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='guild_member';

-- ── Friend ───────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS friend (
  player_id        BIGINT UNSIGNED NOT NULL,
  friend_player_id BIGINT UNSIGNED NOT NULL,
  since_ms         BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (player_id, friend_player_id),
  KEY idx_friend (friend_player_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='friend';

CREATE TABLE IF NOT EXISTS friend_request (
  from_player_id  BIGINT UNSIGNED NOT NULL,
  to_player_id    BIGINT UNSIGNED NOT NULL,
  request_time_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
  status          TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=pending,2=accepted,3=rejected',
  PRIMARY KEY (from_player_id, to_player_id),
  KEY idx_to_player (to_player_id, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='friend_request';
