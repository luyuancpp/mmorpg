INSERT INTO USER (id, create_time, last_login, is_guest, avatar, nickname) VALUES (1, CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP(), 0, null, 'demo');
INSERT INTO user_password (id, user_id, password_hash) VALUES (1, 1, '$2a$10$7v0u6K1XQYQd2fHq6C6V3eB7xjG4rQ6kGz5lNq3Fj1k3A0e9ZkU4i');
