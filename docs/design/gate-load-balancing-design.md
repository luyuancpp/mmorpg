# Gate 负载均衡、票据与重连边界

2026-10-15 按 `6281e4d27` 核对。本文记录当前分配入口与连接约束，本次未执行构建、测试或集群验收。

## 选 Gate 的权威入口

客户端通过 Java gateway 的 HTTP `POST /api/assign-gate` 申请入口，gateway 做限流与 zone
准入后转给 go-zero login 的 `LoginPreGate.AssignGate`。gateway 不选 Gate、不签票；选取最低
负载候选与 HMAC 签发只在 login 实现。

```text
客户端 → HTTP /api/assign-gate → Java gateway → gRPC LoginPreGate.AssignGate
login → 按 zone、可用性与负载选 Gate → 签 GateTokenPayload
客户端 ← gateway ← Gate 地址、payload、signature、deadline（或排队状态）
客户端 → 选中的 Gate：TCP connect → ClientTokenVerifyRequest
Gate → 本地校验签名、目标 Gate 与到期时间 → 允许后续游戏请求
```

选与签有三条路径：

1. 不排队的快路径用 `PickAndSignGateToken`，选取和签发一次完成。
2. 排队 dispatcher 准入时只 `PickGate`；客户端通过 `/api/queue-status` 消费 admit 时才
   `SignGateToken`，TTL 从消费时起算。
3. 携 queue token 重入 `AssignGate` 的 `handleReentry` 同样在消费时签发，沿用已选 Gate。

代码：`go/login/internal/logic/loginpregate/assigngatelogic.go`、`querystatuslogic.go`、
`go/login/internal/logic/pkg/loginqueue/gatetoken.go`。

## 有状态连接与跨 Gate 重连

`SessionInfo`、socket、节点绑定与当前 session 属于承载该 TCP 连接的 Gate，Kafka 下行按目标
Gate 定向；不能把同一条已建立连接的包随意送给另一实例。

**重新连接可以选择另一 Gate。** 新连接先取得匹配目标 Gate 的票据，再握手和 EnterGame；
login 调 `player_locator.Reconnect` 更新 Gate/session，BindSession 与 RoutePlayer 在新 Gate
建立上下文。不能沿用旧 session 就直接向任意 Gate 发游戏包；也不能据“Gate 有状态”推导
“重连必须回原 Gate”。流程见 [player_login_flow.md](player_login_flow.md)。

| 场景 | 当前约束 |
|---|---|
| 已建立的 TCP 连接 | 保持在持有它的 Gate，由该 Gate 感知断线 |
| 相同 Gate 重连 | 仍经过新连接、票据与登录会话更新 |
| 不同 Gate 重连 | 支持；由新连接与 BindSession / RoutePlayer 重建上下文 |
| 跨 zone 换 Gate | 目标 zone 重新入场；同时遵守 owner_epoch / handoff 换手门 |
| 已签给 Gate-A 的票据被 LB 送到 Gate-B | node_id 不匹配，Gate 拒绝 |

## L4 与集群入口

本仓采用“login 分配实例地址 + HMAC 票据”。票据绑定选中的 Gate，因此不能在这个地址后放一个
会把新 TCP 连接随机分给所有 Gate 的共享入口。限制来自票据目标和定向路由，不是所有有状态
网关都天然不能使用 L4，也不是跨 Gate 重连缺失。

K8s 集群外使用 Gate StatefulSet 与每序号 Service `gate-<i>`，保持每实例可寻址；单一
`gate-entry` Service 仅用于 podip 单副本形态。见
[k8s-client-entry.md](k8s-client-entry.md) D87–D90 与
[k8s_gate_exposure_guidance.md](k8s_gate_exposure_guidance.md)。

## Token Details

- payload 至少包含 `gate_node_id`、`zone_id`、`expire_timestamp`、随机 `hmac_session_key`；
  跨区重定向另带玩家和目标 zone 绑定字段，完整字段以 `proto/common/base/message.proto` 为准。
- 签名为 `HMAC-SHA256(secret, serialized_payload)`，按小写 hex 传输。签发使用随机会话 HMAC key，
  但这不等于业务消息逐包 HMAC 已启用，见 [hmac-message-signing.md](hmac-message-signing.md)。
- 普通 AssignGate 票据当前 TTL 为 **10 分钟**（`assigngatelogic.go::gateTokenTTL`），从实际签发
  开始计；不是本页旧版的 60 秒。其他用途的凭证以各自签发点为准。
- login 新配置使用 `Secrets.GateToken`，旧 `GateTokenSecret` 只作兼容回退；Gate 读取
  `BaseDeployConfig.gate_token_secret`。两端必须使用匹配的签名/验签密钥，不能从 Go 的 keyring
  能力推导 C++ Gate 已支持同样的多密钥验签或无缝轮换。密钥值不写入跟踪文档或日志。
- 票据用于短期 TCP 握手；Gate 本地验签不等于全局一次性消费，不能仅凭该流程宣称票据不能重放。

## Gate Enforcement

- `SessionInfo.verified` 默认 false；token 校验通过后置 true，未验证会话不能发送游戏请求。
- `HandleClientTokenVerify` 使用常数时间 HMAC 比较，检查目标 Gate 和到期时间；重定向票据的
  `target_zone_id` 非零时还检查本 zone。玩家/目标绑定继续交给 login 入场链验证。
- **空密钥不默认放行**：`GATE_RUN_MODE` 未设置按 prod；prod 缺密钥拒绝启动，并在连接/握手
  入口再次拒绝。只有显式 dev/test 模式允许空密钥降级，且记录警告。
- 校验实现与启动门禁分别在 `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` 和
  `cpp/nodes/gate/main.cpp`，运行模式策略在 `cpp/nodes/gate/gate_security.h`。

## 客户端地址选择

login 从 etcd NodeInfo 生成候选：按 zone 过滤，优先选择可用的 `client_endpoint`；缺失且
`RequireClientEndpoint=false` 才回落 `endpoint`，为 true 则跳过。随后按地址去重，只留最新
注册，并排除正在 drain 的 Gate。实现为 `buildGateCandidates` 与 `clientendpoint.Select`。

SceneManager 的跨 zone `RedirectToGate` 复用相同客户端地址规则。部署参数
`RequireClientEndpoint=auto` 随入口形态决定，集群外要求显式客户端地址。

Java `GateWatcher` 用于 zone 健康查询，并提供 login 发现；不负责 Gate 选择或票据签发。

## 验证边界

本页修订只对照源码、配置与现有进度记录，没有连接 Redis、启动节点或执行 kind 验收。后续
应分别验证目标 Gate 不符拒绝、prod 缺密钥拒绝、排队兑现后票据有效期、跨 Gate 重连，以及
每实例外部地址与 drain 后重分配，不能以文档已更新替代运行证据。
