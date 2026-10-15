# 在线调试数据读取

2026-10-15 按当前 CLI 修订。实际入口是 `go/data_service/cmd/debug_fetch`,仓库没有 `tools/scripts/debug_fetch_data.ps1` 包装器。

下列命令均先从仓库根进入 Go 模块;示例 ID 要替换成待查实体。服务配置按该工作目录解析,目标环境使用实际配置文件的绝对路径,不要把凭据写进命令行或输出。

```powershell
Set-Location go/data_service
```

## 常用命令

```powershell
# 一名玩家的实时 Redis 数据;先看返回的 field_names,再按真实字段名收窄。
go run ./cmd/debug_fetch -config etc/data_service.yaml -mode player -player 10001

# 指定 Redis hash 字段;名称以实际 field_names 为准。
go run ./cmd/debug_fetch -config etc/data_service.yaml -mode player -player 10001 -fields "PlayerDatabaseData"

# 给二进制字段显式指定 protobuf 类型。
go run ./cmd/debug_fetch -config etc/data_service.yaml -mode player -player 10001 -fields "PlayerDatabaseData" -proto-type PlayerDatabase

# zone 映射与路由信息;server 是 zone 的别名。
go run ./cmd/debug_fetch -config etc/data_service.yaml -mode server -zone 1 -limit 50

# 扫描指定 zone 业务库中与玩家 ID 匹配的行。
go run ./cmd/debug_fetch -db-config ../db/etc/db.yaml -mode player-db -zone 1 -player 10001 -limit 20

# 全局快照库中该玩家的审计快照。
go run ./cmd/debug_fetch -config etc/data_service.yaml -mode snapshot -player 10001 -limit 10

# 只读 SQL:先确认实际表名,再选择字段;SQL 模式连的是 zone 库。
go run ./cmd/debug_fetch -db-config ../db/etc/db.yaml -mode sql -zone 1 -sql "SHOW TABLES"
go run ./cmd/debug_fetch -db-config ../db/etc/db.yaml -mode sql -zone 1 -sql "SELECT player_id FROM player_database LIMIT 20"

# 保存到本地临时目录;下列路径相对于 go/data_service。
go run ./cmd/debug_fetch -config etc/data_service.yaml -mode player -player 10001 -out ../../run/scratch/player-10001.json
```

## 参数与边界

- Redis、路由和快照模式用 `-config`;业务库 SQL 与 `player-db` 用 `-db-config`。默认分别为 `etc/data_service.yaml`、`../db/etc/db.yaml`。
- `-timeout-seconds` 控制整次读取预算,默认 10 秒;`-limit` 默认 20。
- SQL 模式只接受 `SELECT`、`SHOW`、`DESCRIBE`、`EXPLAIN`,不是通用数据库终端。`player-db` 会扫描常见玩家 ID 列,生产读取仍须控制范围。
- 工具尝试自动解码 protobuf、JSON 或文本;识别不足时使用 `-proto-type`。未解码的二进制以 base64 返回。
- 全局快照表不在 zone 业务库里,读取审计快照使用 `snapshot` 模式;不要把旧文档中的 `player_snapshot_entry` 当成 zone 库固定表名。
