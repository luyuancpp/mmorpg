-- Create per-zone databases and grant appuser access.
-- Docker MYSQL_USER only auto-grants on MYSQL_DATABASE (mmorpg),
-- so zone databases need explicit grants.

CREATE DATABASE IF NOT EXISTS zone_1_db;
GRANT ALL PRIVILEGES ON `zone_1_db`.* TO 'appuser'@'%';

-- zone_2_db is used by local two-zone stress tests (`dev start-zones`).
CREATE DATABASE IF NOT EXISTS zone_2_db;
GRANT ALL PRIVILEGES ON `zone_2_db`.* TO 'appuser'@'%';

-- testdb is the local "global" database of data_service
-- (go/data_service/etc/data_service.yaml SnapshotMySQL.DBName). data_service
-- creates transaction_log / player_snapshot / rollback_audit_log / id_segment
-- there at startup (Schema.AutoMigrate) or via `data_service -migrate`; if the
-- database is missing or appuser has no grant, startup logs "schema auto-migrate
-- failed" and AllocateIdSegment (login PlayerId segments) is unavailable.
-- K8s does not use this name: k8s_deploy.ps1 generates 02_k8s_global_db.sql for
-- the cluster-global database instead. NOTE: initdb scripts only run on an empty
-- data volume; an already-initialised local MySQL will not pick this up -- run the
-- two statements below by hand or recreate the volume.
CREATE DATABASE IF NOT EXISTS testdb;
GRANT ALL PRIVILEGES ON `testdb`.* TO 'appuser'@'%';

-- mmorpg_trade:聚宝斋 trade 服务独占库(go/trade/etc/trade.yaml 的 MySQL.DBName)。
-- 按 port-decisions D-14,这里只建库 + 授权;表以 proto/trade/trade_table.proto 为源,
-- 由 go/schemamigrate 建(trade 启动期 Schema.AutoMigrate,或 `trade -f etc/trade.yaml -migrate`),
-- 不要往 mysql-init 里加业务表。appuser 没有全局 CREATE 权限,库必须在 trade 启动前就存在。
-- 注意:与上面 testdb 相同,已初始化过的本地 MySQL 数据卷不会重跑 initdb,
-- 存量卷需要用 root 手工执行下面两句(再 FLUSH PRIVILEGES),或重建数据卷。
-- 本机一键启动(tools/scripts/start_game.ps1)在 MySQL 就绪后预检本库,库不在或 appuser 无权时
-- 跳过 trade 并打印补建命令,不拖垮整个启动。
-- K8s:k8s_deploy.ps1 的 mysql-init-sql ConfigMap 会原样带入本文件,新 PVC 首次 initdb 时同样建库;
-- 已有数据的 PVC 不会重跑,手工补建步骤见 deploy/k8s/README.md
-- (D-14 第 5 条:只登记这一处,不要在 k8s_deploy.ps1 另生成)。
CREATE DATABASE IF NOT EXISTS mmorpg_trade DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON `mmorpg_trade`.* TO 'appuser'@'%';

-- mmorpg_guild:帮会 guild 服务独占库(go/guild/etc/guild.yaml MySQL.DataSource 的库名;
-- go/guild/internal/config.Validate 断言 DSN 库名等于它)。按 port-decisions D-14(§8 已修订:
-- 帮会表一并迁入),这里只建库 + 授权;表以 proto/guild/guild_db.proto 为源,由 go/schemamigrate
-- 建(guild 启动期 Schema.AutoMigrate,或 `guild -f etc/guild.yaml -migrate`),不要往 mysql-init 加帮会表。
-- appuser 没有全局 CREATE 权限,库必须在 guild 启动前就存在。
-- 已初始化过的本地数据卷不会重跑 initdb:用 root 手工执行下面两句(再 FLUSH PRIVILEGES),或重建数据卷;
-- tools/scripts/start_game.ps1 在 MySQL 就绪后预检本库,不就绪时跳过 guild 并打印补建命令。
-- K8s:mysql-init-sql ConfigMap 原样带入本文件,新 PVC 首次 initdb 建库;已有 PVC 手工补建(见 deploy/k8s/README.md)。
CREATE DATABASE IF NOT EXISTS mmorpg_guild DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON `mmorpg_guild`.* TO 'appuser'@'%';

-- mmorpg_friend:好友 friend 服务独占库(go/friend/etc/friend.yaml 的 MySQL.DBName)。
-- 与上面 mmorpg_trade 同一套规矩(port-decisions D-14):**这里只建库 + 授权**;表以
-- proto/friend/friend_table.proto 为源,由 go/schemamigrate 建(friend 启动期 Schema.AutoMigrate,
-- 或 `friend -f etc/friend.yaml -migrate`),不要往 mysql-init 里加业务表。
-- appuser 没有全局 CREATE 权限,库必须在 friend 启动前就存在。
-- ⚠ 2026-09-19 起 deploy/mysql-init/guild_friend_tables.sql **整个文件已删除**:帮会表迁进 mmorpg_guild
--   (帮会二期 B1)、好友表迁进 mmorpg_friend(本条),两边各搬走一半之后它已无内容。
--   那个文件过去把 friend / friend_request / friend_capacity 建进**共享库 mmorpg**(= initdb 的
--   MYSQL_DATABASE;它没有 USE 语句,所以落的**不是 zone 库** —— 存量卷上残留的陈旧表要去 mmorpg 里找,
--   不在 zone_N_db)。两处都建表会让 schemamigrate 把 initdb 建出来的旧结构判成类型漂移
--   (退出码 4,需人工),所以别把建表加回 mysql-init。
-- 注意:与上面 testdb / mmorpg_trade 相同,已初始化过的本地 MySQL 数据卷不会重跑 initdb,
-- 存量卷需要用 root 手工执行下面两句(再 FLUSH PRIVILEGES),或重建数据卷。
-- 本机一键启动(tools/scripts/start_game.ps1)在 MySQL 就绪后预检本库,库不在或 appuser 无权时
-- 跳过 friend 并打印补建命令,不拖垮整个启动。
-- K8s:k8s_deploy.ps1 的 mysql-init-sql ConfigMap 会原样带入本文件,新 PVC 首次 initdb 时同样建库;
-- 已有数据的 PVC 不会重跑,手工补建步骤见 deploy/k8s/README.md
-- (D-14 第 5 条:只登记这一处,不要在 k8s_deploy.ps1 另生成)。
CREATE DATABASE IF NOT EXISTS mmorpg_friend DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON `mmorpg_friend`.* TO 'appuser'@'%';

-- mmorpg_player_inbox:玩家持久收件箱 player_inbox 服务独占库(go/player_inbox/etc/player_inbox.yaml 的 MySQL.DBName;
-- docs/design/player-inbox-service.md §1.4、D-14:不借 data_service 的全局库)。与上面 mmorpg_trade 同一套规矩:
-- **这里只建库 + 授权**;表(player_inbox_stream / player_inbox_item / player_inbox_audit)以
-- proto/player_inbox/player_inbox_table.proto 为源,由 go/schemamigrate 建(启动期 Schema.AutoMigrate,
-- 或 `player_inbox -f etc/player_inbox.yaml -migrate`),不要往 mysql-init 里加业务表。
-- appuser 没有全局 CREATE 权限,库必须在 player_inbox 启动前就存在。
-- 注意:已初始化过的本地 MySQL 数据卷不会重跑 initdb,存量卷需要用 root 手工执行下面两句(再 FLUSH PRIVILEGES),
-- 或重建数据卷。本机一键启动(tools/scripts/start_game.ps1)在 MySQL 就绪后预检本库,库不在或 appuser 无权时
-- 跳过 player_inbox 并打印补建命令,不拖垮整个启动。
-- K8s:mysql-init-sql ConfigMap 原样带入本文件,新 PVC 首次 initdb 建库;已有数据的 PVC 不会重跑,
-- 手工补建步骤见 deploy/k8s/README.md「玩家收件箱 player-inbox」(D-14 第 5 条:只登记这一处)。
CREATE DATABASE IF NOT EXISTS mmorpg_player_inbox DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON `mmorpg_player_inbox`.* TO 'appuser'@'%';

-- 新服务只在此处预建独占库，业务表仍由各服务 -migrate 创建。
CREATE DATABASE IF NOT EXISTS mmorpg_mail DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON `mmorpg_mail`.* TO 'appuser'@'%';
CREATE DATABASE IF NOT EXISTS mmorpg_rank DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON `mmorpg_rank`.* TO 'appuser'@'%';

FLUSH PRIVILEGES;
