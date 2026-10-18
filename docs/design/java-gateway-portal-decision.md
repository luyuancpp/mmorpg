# Java Gateway Portal Decision (2026-04-14)

## Decision
Use Java (Spring Boot) for the unified HTTP gateway/portal that handles:
- 公告 CRUD
- 热更版本检查
- CDN 签名下发
- 服务器列表
- 运维后台 (RBAC + CRUD)

## Why Java over Go
- **统一入口 + 过滤器链**: Spring MVC HandlerInterceptor / WebFlux WebFilter — 原生链式，声明式路由级别粒度。
- **限流**: Bucket4j / Resilience4j，开箱即用，支持 Redis 分布式限流。
- **认证**: 已有 `sa_token_node`，Sa-Token 天然支持路由拦截、踢人、Token 续签。
- **运维后台**: Sa-Token 权限注解 + MyBatis-Plus / JPA，快速出 CRUD。Go 没有对标方案。
- **决定性因素**: 运维后台 RBAC + CRUD 开发效率，Java 生态远高于 Go。

## Architecture

```
┌─────────────────────────────────────────────┐
│  Spring Boot 统一网关/门户 (单进程)          │
│                                             │
│  Filter Chain:                              │
│    → RateLimiter (Bucket4j + Redis)         │
│    → Sa-Token Auth Interceptor              │
│    → RBAC Permission Check                  │
│                                             │
│  Controllers:                               │
│    /api/announce/*        公告 CRUD          │
│    /api/server-list       服务器列表         │
│    /api/hotfix/check      热更版本检查       │
│    /api/cdn/sign          CDN 签名下发       │
│    /admin/*               运维后台 (Vue前端) │
│                                             │
│  Downstream:                                │
│    → Redis (限流计数、缓存)                  │
│    → MySQL (公告、配置持久化)                │
│    → gRPC → 现有 Go/C++ 服务 (需要时)       │
└─────────────────────────────────────────────┘
```

## Boundary

| Layer | Tech | Responsibility |
|-------|------|----------------|
| HTTP gateway/portal | Java Spring Boot | 公告、热更、CDN签名、服务器列表、运维后台 |
| Game-internal RPC | Go (go-zero) | 登录、场景管理、数据服务、玩家定位 |
| Runtime nodes | C++ | Gate、Scene、Centre 节点 |

## Key Points

1. **复用 `sa_token_node`**: 现有 Java auth 已跑通 Sa-Token + gRPC，直接在此基础上扩展。
2. **运维后台是决定性因素**: Go 做 CRUD 后台 + 权限管理的开发效率远低于 Spring Boot + Sa-Token 注解。
3. **与 Go 服务的边界**: Java 门户负责面向客户端/运维的 HTTP 层；Go 服务继续负责游戏内 gRPC 逻辑，通过 gRPC 互调。
4. **前端**: 运维后台前端用 Vue + Element Plus，Java 后端纯 REST API，前后端分离。

## Location
- Java gateway code: `java/gateway_node/` (or extend `java/sa_token_node/`)

## 2026-10-17：目录表与 JPA 类型契约

`gateway_node` 的 `ZoneConfig.zoneId` 与 `ZoneWhitelist.zoneId` 保持 `Long`，SQL 两列统一为 `BIGINT`，
可以无损承载协议 `uint32` 的完整正值范围。模块 `schema.sql` 与既有
`deploy/mysql-init/gateway_tables.sql` 使用同一形态：`manual_status INT`、`capacity INT`、
`recommended BIT(1)`。启动继续使用 `ddl-auto=validate`；不能改为 `update` 掩盖结构漂移。

旧模块建出的 `INT UNSIGNED` 区号列需要显式迁移；`CREATE TABLE IF NOT EXISTS` 不会修复已有表。
先停目录写者、备份并核对列形状及数据范围：区号在 `1..4294967295`、状态在 `0..3`、
容量不超过 Java `int` 上界、推荐标记只有 `0/1`。再执行以下加宽/对齐操作并回读结构，最后启动新版本验证。
两张表的 DDL 分别提交；中途失败时先核对已完成部分，不能删除目录或重新播种覆盖数据。
以下原地 `ALTER` 适用于支持该类型变更的表。TiDB 的存量 `INT UNSIGNED` 聚簇主键
可能以 `Unsupported modify column: this column has primary key flag` 拒绝第一条，
此时必须停下并回读结构；不能循环重试、关闭 `validate` 或删除旧表。该形态需要独立评审的
影子表复制及切换方案，保留原表和完整数据，不能把下列语句当成已经验证可用的 TiDB 迁移。

```sql
ALTER TABLE zone_config
    MODIFY COLUMN zone_id BIGINT NOT NULL,
    MODIFY COLUMN manual_status INT NOT NULL DEFAULT 0 COMMENT '0=OPEN, 1=MAINTENANCE, 2=CLOSED, 3=PREVIEW',
    MODIFY COLUMN capacity INT DEFAULT 5000,
    MODIFY COLUMN recommended BIT(1) DEFAULT 0;
ALTER TABLE zone_whitelist MODIFY COLUMN zone_id BIGINT NOT NULL;
```

本轮仅在创建收据登记的 `asset_arrival_20261017_directory` 上，针对两条区服种子和空白名单，
从正式 DDL 建固定影子表、复制全部列并核对摘要/行数，单条 `RENAME TABLE` 切换并保留两张旧表。
TiDB 上本次切换成功，切前、影子、切后和备份的种子摘要相同；未删除旧表或重新播种。
这只是已停写、无玩家数据的隔离目录修复证据，生产目录需单独预演复制成本、写者冻结、切换与回滚，
不能直接套用本轮固定表名或把本次结果当成生产迁移验证。

`GatewaySchemaContractTest` 直接执行正式 `schema.sql`，然后用 Hibernate `validate` 核对全部三个实体，
并通过实体将 `4294967295` 区号写入、回读两张目录表。默认 H2 只验证接口契约；显式
`GATEWAY_SCHEMA_TIDB_URL=jdbc:mysql://127.0.0.1:4000/`、测试账号及口令环境变量启用的用例
会创建、清理随机 `gateway_schema_*_test` 库，提供独立真实 TiDB 证据，不接管已有数据库。

Java 全服务版的 `game-gateway-store` 使用 MyBatis、`ZoneRow`/`WhitelistRow` 的 `int` 区号与
`INT UNSIGNED` 列，没有 JPA 启动类型校验，不存在本次 `Long` 与 JDBC `INTEGER` 不匹配的启动问题，
因此不为本次修复改动其 DDL。其有符号 `int` 接口能表达的区号范围仍是独立约束；
本次 `gateway_node` 的 `uint32` 上界回归不能作为该版本的完整上界证据。
