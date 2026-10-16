# go/db 的 MySQL 凭据与建库边界

**起始决策：**2026-04-12；**现状修订：**2026-10-15。

> TiDB 化后的用户/权限模型与凭据注入仍需按 [global-data-layer-tidb-decision.md](./global-data-layer-tidb-decision.md) §D9 落地。本文修正当前事实，不表示已完成最小权限改造。

## 当前配置与访问方

`go/db/etc/db.yaml` 的本地默认 `ServerConfig.Database.User` 仍为 `root`。这是现有配置事实；不能由此推导所有环境必须复用 root 或相同密码。

go/db 消费玩家 DBTask 并写玩家库。data_service、friend、guild、trade 和 Login 的密码认证存储也有各自的 MySQL 连接，不经 go/db 中转。friend / guild / trade 分别使用独占逻辑库，相关迁移与权限以各服务配置及设计为准。

## 当前建库流程

go/db 启动时由 `ZoneId` 派生 `zone_{N}_db`，校验外部库名白名单，再连接已有库。默认 `AutoCreateDatabase=false`、`AutoMigrateSchema=false`；缺库时拒启并提示先迁移，防止填错 ZoneId 后静默建空库并写入玩家数据。

部署顺序：

1. 配置 MySQL 及部署所需凭据，核对目标库白名单。
2. 在 `go/db` 目录用目标环境的 yaml 执行迁移入口：`go run ./cmd/migrate -f <yaml> -command plan`，检查后执行 `-command up`。
3. 新库需要显式建库时，在 `up` 命令加 `-create-database`；该入口仍要求目标库通过白名单，并要求执行账号有相应建库权限。
4. 迁移完成后启动 go/db，运行期不依赖自动建库/改表。

迁移入口是 `go/db/cmd/migrate`。业务启动策略见 `go/db/internal/logic/pkg/proto_sql/db.go`：只有显式打开 `AutoCreateDatabase`（或环境变量 `DB_AUTO_CREATE_DATABASE`）才允许兼容旧的自动建库行为；该兼容开关不是生产默认流程。

## 凭据来源

- 本地 MySQL 初始化：`deploy/docker-compose.yml` 的 `MYSQL_ROOT_PASSWORD` / `MYSQL_USER` 等配置。
- K8s MySQL：`deploy/k8s/manifests/infra/mysql.yaml` 及部署时的凭据配置。
- go/db 运行配置：`ServerConfig.Database.User` / `Passwd`；部署渲染后的配置必须与目标数据库授权匹配。
- 其他服务：使用自身配置的连接与独占库授权，不能因 go/db 默认 root 就替换为同一账号。

凭据值不在本文复制，跨环境也没有“统一 root 密码”的要求。运行账号与迁移账号的最小权限拆分仍需单独落地；仅修正文档不改变当前账号配置。

## 历史决策

2026-04-12 曾因 `appuser` 缺少 `CREATE DATABASE` 权限（1044），把 go/db 改为 root 以支持启动自动建区库。之后建库/迁移收口到显式入口，原文“db 是唯一 MySQL 访问方”“分离管理连接无安全收益”和“首次启动必自动建库”均不再适用。
