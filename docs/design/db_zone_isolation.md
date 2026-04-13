# DB Zone Isolation (2026-04-11)

## 设计

每个 zone 部署独立的 db 服务实例，Kafka topic 和 MySQL 数据库名均从 `ZoneId` 动态派生：

- **Kafka topic**: `db_task_zone_{zone_id}` (e.g. `db_task_zone_1`, `db_task_zone_101`)
- **MySQL database**: `zone_{zone_id}_db` (e.g. `zone_1_db`, `zone_101_db`)

## 派生逻辑

Topic 和 DBName **不在 yaml 中硬编码**，由启动时从 `ZoneId` 自动拼接：

| 服务 | 配置字段 | 派生函数 |
|------|---------|---------|
| go/db | `Config.ZoneId` | `config.DbTaskTopic()` / `config.ZoneDBName()` — db.go main 中派生 |
| go/login | `Node.ZoneId` | `config.DbTaskTopic()` — login.go main 中派生 |
| C++ scene | `GetZoneId()` (from NodeInfo) | `GetDbTaskTopic(zoneId)` — player_lifecycle.cpp |

## 关键文件

- `go/db/internal/config/config.go` — ZoneId 字段 + DbTaskTopic() + ZoneDBName()
- `go/db/db.go` — 启动时派生 topic 和 DBName
- `go/login/internal/config/config.go` — DbTaskTopic()
- `go/login/login.go` — 启动时派生 topic
- `cpp/libs/services/scene/player/constants/player.h` — GetDbTaskTopic()
- `tools/scripts/k8s_deploy.ps1` — K8s ConfigMap 中注入 ZoneId

## 多 zone 回档优势

- 单 zone 回档只需操作对应 topic + 对应数据库，不影响其他 zone
- Kafka topic 删重建命令：`kafka-topics.sh --delete --topic db_task_zone_{id}`
