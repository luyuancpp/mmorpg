# 登录与 Gate 分配迁移方案

## 状态：Phase 1 已完成 (2026-04-05)

## 当前架构（Phase 1 之后）
- **Gateway 服务** (`go/gateway/`) 提供 HTTP REST 接口 — 面向客户端的公共 API 层
- **Login 服务** (`go/login/`) 保留原有 gRPC `AssignGate` RPC，供内部/压测机器人使用
- 客户端流程：`GET /api/server-list` → 选区 → `POST /api/assign-gate {zone_id}` → 获取 `{ip, port, token}` → TCP 连接 Gate
- Gate 验证 token（`ClientTokenVerifyRequest`）不变 — 仍为 HMAC-SHA256 格式
- 压测机器人已支持调用 `/api/server-list` 自动选区（`zone_id=0` 时）

## Phase 1：HTTP 平台接口（已完成）
- 在 `go/gateway/` 创建 **Gateway 服务**（15 个文件，go-zero REST 服务器）
- 接口：
  - `GET /api/server-list` → 返回区服列表（id、名称、状态、推荐标志）
  - `POST /api/assign-gate` → 接收 `{zone_id}`，返回 `{ip, port, token, deadline}`
  - `GET /api/health` → 健康检查
- Gateway 通过 etcd 发现 Gate 节点，使用区服无关前缀 `GateNodeService.rpc/`
- HMAC token 签名复用 Login 的算法（共享密钥）
- 已部署：每个区 2 副本（zones 3-10），共 16 pods，镜像 `ghcr.io/luyuancpp/mmorpg-gateway:v1`
- Login 的 REST 路由已移除；Login 现在是纯 gRPC 服务
- 原有 gRPC `AssignGate` 保留，用于内部/机器人向后兼容

### Phase 2：SDK 鉴权
- 接入**第三方 OAuth**（微信、Apple、Google 等）
- 流程：客户端 → SDK → 获取 `access_token` → 发送至我们的 HTTP API 验证
- 服务端调用 SDK 服务器验证 token（服务端到服务端回调）
- 验证通过后，签发内部 `session_token` 用于 Gate 分配
- Java `sa_token_node` 可能作为鉴权桥梁，或被替代

### Phase 3：Session Key 升级
- 当前：HMAC token 绑定 `gate_node_id`，TTL 60 秒
- 目标：session_key 携带更丰富元数据（account_id、zone_id、客户端 IP 绑定、防重放 nonce）
- 考虑使用短期 JWT 或存入 Redis 的不透明 token

## 涉及的关键文件
- `go/gateway/` — 新 Gateway 服务（15 个文件，go-zero REST）
- `go/login/internal/logic/pregate/getgatelistlogic.go` — 当前 AssignGate（保留供内部使用）
- `go/login/internal/svc/gate_watcher.go` — Gate 发现（Gateway 复用）
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` — token 验证（Phase 3 升级新格式）
- `robot/gate.go`、`robot/main.go` — 压测机器人（已添加 HTTP 路径与自动选区）
- `robot/config/config.go` — 增加 `ZoneID` 字段
- `deploy/k8s/robot_stress.ps1` — K8s 机器人 ConfigMap 已更新 gateway 字段

## 迁移策略
- 先做 Phase 1：纯增量，无破坏性变更
- 机器人保留 gRPC 路径；真实客户端走 HTTP 路径
- Gate token 验证在过渡期同时支持旧 HMAC 和新 session_key 格式

## 行业参考：各大厂架构做法

| 公司 | 平台鉴权 | 接入层 (Gateway) | 网关 (Gate) | 游戏服 |
|------|---------|-----------------|-------------|--------|
| 腾讯 (MSDK) | MSDK → 微信/QQ OAuth → 拿 openid+token | 独立 HTTP 服务，验 MSDK token，发 session_key | TCP，验 session_key | 角色逻辑 |
| 网易 (URS) | URS SDK → 网易账号 → token | 独立 HTTP "登录网关" | TCP | 角色逻辑 |
| 米哈游 | HoYoverse SDK → token | HTTP API（国际版用 dispatch server） | KCP/TCP | 角色逻辑 |
| 莉莉丝 | 自研 SDK → token | HTTP Gateway (Go) | TCP Gateway | Game Server |

### 关键观察
- 所有大厂都将**平台鉴权**（SDK token 验证）与**网关连接**（TCP/KCP 会话）分离。
- 接入层始终是位于 SDK 鉴权和游戏 Gate 之间的**独立 HTTP 服务**。
- 这与我们 Phase 2 的目标吻合：Gateway HTTP API 验证 SDK token，签发 session_key，客户端携带 session_key 连接 Gate。
- 米哈游的 "dispatch server" 模式值得关注 — 它动态地将玩家路由到不同的区域服务器集群。
- KCP（米哈游/原神使用）是一种面向延迟敏感游戏优化的可靠 UDP 协议。
