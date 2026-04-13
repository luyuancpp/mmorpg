# 登录网关分配改造计划：当前架构 → 大厂标准架构

> **状态**: 规划中（未开始）  
> **日期**: 2026-04-05

---

## 1. 当前架构

```
客户端 ──gRPC──► Login服务 (AssignGate, 传zone_id)
                     │
                     ├─ 从etcd获取所有Gate节点
                     ├─ 按zone过滤，选最空闲Gate
                     ├─ HMAC-SHA256签发token (gate_node_id + zone_id + expire)
                     └─ 返回 {ip, port, token_payload, token_signature, ttl=60s}

客户端 ──TCP──► Gate节点 (ClientTokenVerifyRequest 握手)
                     │
                     └─ 后续所有RPC (Login/CreatePlayer/EnterGame) 走 Gate → Login gRPC
```

**特点**:
- 无服务器列表接口，客户端直接传 `zone_id`
- 无第三方SDK认证，密码在Login服务内部校验
- AssignGate 通过 gRPC 暴露，非 HTTP

**关键文件**:
| 文件 | 职责 |
|------|------|
| `go/login/internal/logic/pregate/getgatelistlogic.go` | AssignGate 实现 |
| `go/login/internal/svc/gate_watcher.go` | etcd Gate发现 |
| `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` | Gate侧token验证 |
| `robot/login.go` | 机器人客户端登录流程 |

---

## 2. 大厂标准架构

```
客户端 ──HTTPS──► SDK服务器 (微信/QQ/Apple/Google OAuth)
                     └─ 返回 access_token

客户端 ──HTTPS──► 平台HTTP API
                     ├─ GET  /api/server-list   → 区服列表 (名称/状态/负载/推荐)
                     └─ POST /api/assign-gate    → 验证token + 分配Gate
                          └─ 返回 {gate_ip, gate_port, session_key, ttl}

客户端 ──TCP/KCP──► Gate节点 (session_key握手)
                     └─ 后续所有游戏协议走此长连接
```

**优势**:
- 服务器列表可CDN缓存，维护期间不需要游戏服启动即可展示维护公告
- HTTP层便于接WAF/防刷/限流
- SDK认证标准化，对接多渠道
- session_key可绑定更多信息（client_ip、防重放nonce）

---

## 3. 差异对比

| 维度 | 当前 | 大厂标准 |
|------|------|---------|
| 获取Gate | gRPC `AssignGate` | HTTPS `POST /api/assign-gate` |
| 服务器列表 | 无，客户端直传zone_id | HTTPS `GET /api/server-list` |
| 账号认证 | Login内部校验密码 | SDK OAuth + 服务端回调校验 |
| 凭证格式 | HMAC-SHA256 (gate_node_id+zone_id+expire) | session_key (JWT或Redis opaque token) |
| 连接Gate | TCP + HMAC token | TCP/KCP + session_key |
| 客户端首连协议 | gRPC (protobuf) | HTTPS (JSON) |

---

## 4. 改造分三期

### Phase 1: HTTP平台API（服务器列表 + 网关分配）

**目标**: 在不破坏现有流程的前提下，新增HTTP接口层

**新增接口**:
```
GET  /api/server-list
Response: {
  "zones": [
    {"zone_id": 1, "name": "一区", "status": "normal", "load": "light", "recommended": true},
    {"zone_id": 2, "name": "二区", "status": "maintenance", "load": "full"}
  ]
}

POST /api/assign-gate
Request:  {"zone_id": 1, "access_token": "..."}
Response: {"gate_ip": "1.2.3.4", "gate_port": 33333, "session_key": "...", "ttl_seconds": 60}
```

**实现选择**:
- 方案A: 在Login服务中新增HTTP handler（go-zero支持同时暴露RPC+HTTP）
- 方案B: 独立HTTP网关服务，内部调用Login gRPC
- 方案C: 利用现有Java `sa_token_node` 做HTTP层

**兼容策略**:
- 保留现有gRPC `AssignGate`，Robot客户端继续用gRPC
- 真实游戏客户端走新HTTP接口

### Phase 2: SDK认证集成

**目标**: 对接第三方OAuth，替换内部密码校验

**流程**:
```
客户端 → SDK登录 → 拿到 sdk_access_token
客户端 → POST /api/assign-gate {zone_id, sdk_type, sdk_token}
服务端 → 回调SDK服务器验证token (server-to-server)
服务端 → 验证通过 → 查找/创建账号 → 分配Gate → 返回session_key
```

**受影响**:
- 新增SDK回调模块（每个渠道一个adapter）
- 账号系统需支持多渠道绑定（一个账号绑定微信+Apple等）
- `LoginRequest` 中的 `account/password` 字段逐步废弃

### Phase 3: Session Key升级

**目标**: 增强token安全性

**当前**: HMAC token = `{gate_node_id, zone_id, expire_timestamp}` + HMAC签名

**升级方向**:
- 增加 `account_id`、`client_ip` 绑定
- 增加 anti-replay nonce
- 可选方案: 短期JWT (自包含, Gate可本地验证) 或 Redis opaque token (Gate需查Redis)

**Gate侧兼容**:
- 过渡期Gate同时支持旧HMAC和新session_key格式
- 通过token前缀或版本字节区分

---

## 5. 迁移策略

1. **Phase 1 纯增量**，不改动现有gRPC路径，零风险
2. Robot客户端保持gRPC直连；真实客户端走HTTP
3. Gate token验证过渡期支持双格式
4. Phase 2 可独立于Phase 3执行，SDK认证和session_key升级互不依赖
5. 每期改造完成后通过压测验证（复用现有K8s robot压测框架）
