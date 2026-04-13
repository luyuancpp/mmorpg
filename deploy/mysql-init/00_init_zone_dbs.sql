-- Create per-zone databases and grant appuser access.
-- Docker MYSQL_USER only auto-grants on MYSQL_DATABASE (mmorpg),
-- so zone databases need explicit grants.

CREATE DATABASE IF NOT EXISTS zone_1_db;
GRANT ALL PRIVILEGES ON `zone_1_db`.* TO 'appuser'@'%';

FLUSH PRIVILEGES;
