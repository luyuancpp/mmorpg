CREATE TABLE users (
                       id BIGINT PRIMARY KEY AUTO_INCREMENT,
                       create_time DATETIME,
                       last_login DATETIME,
                       is_guest TINYINT DEFAULT 0,
                       avatar VARCHAR(255),
                       nickname VARCHAR(64)
);

CREATE TABLE user_password (
                               user_id BIGINT PRIMARY KEY,
                               password_hash VARCHAR(255),
                               CONSTRAINT fk_user_password FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE user_phone (
                            user_id BIGINT,
                            phone VARCHAR(32) UNIQUE,
                            CONSTRAINT fk_user_phone FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE user_oauth (
                            id BIGINT PRIMARY KEY AUTO_INCREMENT,
                            user_id BIGINT,
                            provider VARCHAR(32),  -- weixin / qq / google / apple …
                            openid VARCHAR(128),   -- 或 sub
                            UNIQUE(provider, openid),
                            CONSTRAINT fk_user_oauth FOREIGN KEY (user_id) REFERENCES users(id)
);
