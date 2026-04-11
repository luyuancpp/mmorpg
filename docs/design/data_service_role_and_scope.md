# data_service 定位与边界 (2026-04-11)

## 定位
`go/data_service/` 是**跨区(cross-zone)运行时数据代理**，服务于玩家游戏过程中的跨区场景数据访问和运维工具。

## 核心职责
1. **跨区数据透明访问** — Scene Server 调 gRPC 统一接口，data_service 按 `player_id → home_zone_id → Redis` 路由，Scene 无需知道玩家归属哪个区
2. **Load/Save** — `LoadPlayerData`, `SavePlayerData`, `GetPlayerField`, `SetPlayerField`
3. **玩家区归属** — `RegisterPlayerZone`, `GetPlayerHomeZone`
4. **快照与回档** — `CreateSnapshot`, `DiffSnapshot`, `RollbackPlayer/Zone/All`
5. **GM/运维** — `BatchRecallItems`, `QueryTransactionLog`, `CreateEventSnapshot`

## 与 Login 的关系：无关
- Login 使用 Kafka 直连 db 服务读写玩家数据（`sync_loader.go` → Redis check → Kafka DBTask → BLPop）
- Login 只操作玩家所在区的 Redis，不需要跨区路由
- 在登录时间敏感路径上加一层 gRPC 到 data_service 会增加延迟，无收益
- **结论：Login 不接 data_service，两者独立**

## 谁用 data_service
| 调用方 | 场景 |
|--------|------|
| Scene Server (C++) | 跨区玩家在非本区 Scene 上游戏时，通过 data_service 透明读写 home zone Redis |
| GM/运维工具 | 快照、回档、批量回收、交易日志查询 |
| 未来跨区功能 | 跨区邮件、拍卖行等需要访问玩家 home zone 数据的系统 |

## 架构位置
```
Player → Gate → Scene Server (任意区)
                    │
                    │ gRPC (区无关的统一接口)
                    ▼
              data_service (Go, go-zero)
                    │
                    │ 按 player_id → home_zone_id 路由
                    ▼
              Redis (玩家 home zone)
```

## 关键文件
- `go/data_service/data_service.go` — 入口
- `go/data_service/internal/logic/data_logic.go` — Load/Save/Field 操作
- `go/data_service/internal/logic/rollback_logic.go` — 回档
- `go/data_service/internal/logic/snapshot_logic.go` — 快照
- `go/data_service/internal/logic/recall_logic.go` — GM 批量回收/交易日志
- `go/data_service/internal/routing/router.go` — player_id → zone → Redis 路由
