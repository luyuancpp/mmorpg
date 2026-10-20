# Gate 负载均衡、票据与重连边界

2026-10-15 按 `6281e4d27` 核对。本文记录当前分配入口与连接约束，本次未执行构建、测试或集群验收。

## 选 Gate 的权威入口

客户端通过 Java gateway 的 HTTP `POST /api/assign-gate` 申请入口，gateway 做限流与 zone
准入后转给 go-zero login 的 `LoginPreGate.AssignGate`。gateway 不选 Gate、不签票；login（assign-gate 入口）与
scene_manager（跨 zone 重定向）各自按负载选 Gate 并签发，挑选规则共用 `go/shared/gatepick`（见下文「gate 目录怎么读、怎么挑」）。

```text
客户端 → HTTP /api/assign-gate → Java gateway → gRPC LoginPreGate.AssignGate
login → 按 zone、可用性与负载选 Gate → 签 GateTokenPayload
客户端 ← gateway ← Gate 地址、payload、signature、deadline（或排队状态）
客户端 → 选中的 Gate：TCP connect → ClientTokenVerifyRequest
Gate → 本地校验签名、目标 Gate 与到期时间 → 允许后续游戏请求
```

选与签有三条路径：

1. 不排队的快路径用 `PickAndSignGateToken`，选取和签发一次完成。
2. 排队 dispatcher 准入时只挑 Gate（批内 `gatePicker`，经 `newBatchPickFn`，不走 `PickGate`）、不签；
   客户端通过 `/api/queue-status` 消费 admit 时才 `SignGateToken`，TTL 从消费时起算。
3. 携 queue token 重入 `AssignGate` 的 `handleReentry` 同样在消费时签发，沿用已选 Gate。

代码：`go/login/internal/logic/loginpregate/assigngatelogic.go`、`querystatuslogic.go`、
`go/login/internal/logic/pkg/loginqueue/gatetoken.go`。

### gate 目录怎么读、怎么挑（2026-10-18 起；已落码，未编译、未测试，待 Codex 验证）

**读目录**：login 的 `node.NodeWatcher` 对本 zone 的 gate 前缀做 list-watch，维护一份进程内镜像
（`go/login/internal/logic/pkg/node/node_cache.go`、`watcher.go`）。AssignGate 快路径与排队 dispatcher
经 `CandidatesForZone` → `NodeWatcher.Nodes(ctx)` 读镜像，不再每个请求打一次 etcd 前缀 Range。

- **新鲜度按每台 gate 各自计时**：镜像记下每台 gate 最近一次被刷新（收到它的事件或全量同步）的时刻，整体新鲜度
  取最久没刷新的那台；镜像为空时取最近一次事件或同步的时刻。不用整个前缀共用一个时间戳：多 gate 时别的 gate 的事件
  不能替一台没刷新的 gate 作证。
- 镜像还没完成第一次全量同步，或有 gate 超过 `cacheStaleAfter`（15s）没刷新，`Nodes` 回落直读 etcd：一次线性一致
  Range，超时取请求 ctx 截止时间与 `ServiceDiscoveryTimeout` 中较早者。回落路径的行为与改动前相同。
- 重新全量同步的时机：watch 出错、被压缩、丢 leader（`WithRequireLeader`）；建流超过 `cacheResyncAfter`（12s）
  仍未确认；有 gate 超过 12s 没刷新（gate 每 10s 无条件上报一次，健康时不会触发；某台漏报或还没开始上报时
  每约 12s 多一次 Range）。空 zone 按同样间隔周期重同步，镜像保持新鲜。每轮 watch 用独立的 `clientv3.Watcher`、
  结束时 Close，卡在半开连接上的流随之拆掉；login 的 etcd 客户端另开 gRPC keepalive（10s 探、5s 超时）。
- 两个时限都按 gate 上报周期 10s 推（`cpp/nodes/gate/main.cpp` 的 `playerCountReportTimer`），改上报周期
  必须同步改 `gateReportPeriod`。按每台计时，镜像里任一台 gate 的数据最多比 etcd 落后约 5s（它错过的那次上报
  之后最多 5s 就被重同步补上或判过期），人数最坏晚约 5s 出现，仍在排队在途占位宽限 `defaultHandoffGrace`（20s）之内。
- **gate 排空监视不读镜像**，仍用直读的 `FetchAllNodes`：写 drained 会让脚本删 Pod，读不到 etcd 就本轮不判，
  不能拿可能过期的镜像判定。
- **排空脚本的配合**：`tools/scripts/k8s_gate_drain.ps1` 确认旧记录在 etcd 里消失后，再等契约
  `LoginMirrorSettleSeconds`（20s，必须大于 `cacheStaleAfter`，契约测试钉住）才清排空标记：watch 卡住时 login
  镜像可能还留着旧记录，标记清早了会把玩家分到已删 Pod 的地址上。
- 指标 `login_gate_directory_reads_total{source}`，source 取 `cache` / `etcd_unsynced` / `etcd_stale`。
  稳态应几乎全是 cache；`etcd_stale` 持续不为 0 说明 etcd 不可达或 watch 反复卡住，登录又回到逐请求打 etcd。

**挑 gate**（两条规则；随机二选一的实现只有一份，在 `go/shared/gatepick` 的 `TwoChoices`，login 快路径与
scene_manager 跨 zone 重定向共用；Java 版 `game-discovery` 的 `GatePicker.pick` 同规则）：

1. 快路径 `PickGate`：在候选里随机抽两台不同的 gate，取人数少的那台，同人数取第一次抽中的；只有一台就是它，
   恰好两台时等于取最少。同人数不按 NodeID 定胜负：开新区第一个上报窗口里所有 gate 都报 0，按 NodeID 定会让
   小号 gate 拿双倍份额、NodeID 最大的那台一个人都分不到。
   原来是确定性取全局最少：gate 人数每 10s 才上报一次，快路径各请求互相看不到别人刚分出去的人，一个上报
   周期里本 zone 的新连接会全部落到同一台（容量模型 §2.1「PickGate 集中度」）。随机二选一下，滞后窗口里
   最闲那台最多拿 2/n 的新连接（3 台 67%，10 台 20%）；最忙的那台只有和同样忙的 gate 比时才会被选中。
   **当前 K8s 默认每 zone 2 台 gate**（`k8s_deploy.ps1` 的 `GateReplicas = 2`、`deploy/k8s/zones.json`）：两台人数不等时
   与旧规则相同；全体同人数（开新区第一个上报窗口）时从全落 NodeID 最小的一台变为两台各半；之后的窗口里羊群依旧，
   充分摊开要每 zone 至少 3 台。验证这一项必须按至少 3 台部署，并按 gate 统计快路径下发分布。
2. 排队 dispatcher 一拍放一批，用确定性的 `gatePicker`：在同一份候选上逐条挑并记账（负载 = 上报人数 +
   本拍已分给它的人数，同负载 NodeID 小的在前），先填最闲的，填平之后轮转。账不跨拍。

**scene_manager 跨 zone 重定向**（`go/scene_manager/internal/logic/gate_redirect.go`，2026-10-18 起；已落码，未编译、未测试）：
顺序与 login 的 `CandidatesForZone` 一致——选客户端地址并按地址去重 → 剔除排空中的 gate（全部排空时忽略标记并打 ERROR）
→ 随机二选一。此前是 `sort.Slice` 确定性取最少（同数不稳定）且不过滤排空。排空标记由 `drainingGateIDs` 一次 MGET 读，
读失败放行；键名 `gate:%d:draining` 在 scene_manager 里抄了一份，由 `tools/scripts/tests/k8s_gate_drain.tests.ps1`
钉住与 login、排空脚本三处一致。scene_manager 经**自己的** Redis 连接读标记，今天与 login 的 `Node.RedisClient` 是同一实例；
**拆 Redis（G4a）把排空标记挪到 redis-queue 时，scene_manager 这条读也要一起指过去**（G4a 修订稿需把 scene_manager 列为
`gate:{id}:draining` 的读者；2026-10-19 注：已在 `shared-redis-split-by-purpose.md` §2.2 A 表、§3.2.1 约束 1 与 §20.5 登记），否则读不到标记、退回不过滤。另一个隐含前提：go-zero 的 `RedisConf` 没有 DB 字段，scene_manager
只能读 DB 0；login 的 `Node.RedisClient.DB` 改成非 0 时要同步处理。排空过滤的纯逻辑在 login（`loginqueue.FilterDrainingGates`）
与 scene_manager（`filterDrainingTargets`）各一份：两边的读客户端不同（go-redis / go-zero）、候选类型不同，login 那份又被
`k8s_gate_drain.tests.ps1` 按文本钉住，本次没有合并进 go/shared。

**还没做**（容量模型 A0-7 的其余部分与评审登记）：

- 新鲜度过滤：死掉的 gate 在 etcd 键 TTL（180s）到期前仍在候选里，镜像与直读都一样；
- 跨请求的本地分配累加：快路径只靠随机二选一摊开，不记「本周期已分给某台多少人」；
- dispatcher 跨拍仍从同一份滞后人数起算，同一个上报周期内连续几拍都会先填同一台最闲的 gate；
- 快路径不感知单台 gate 的连接上限（`GateMaxConnections`，满了 gate 直接断开）：每 zone 至少 3 台、其中 m 台
  顶到上限、区级还有名额时，约 m(m-1)/(n(n-1)) 的快路径请求会被派到满载 gate（旧规则在数据新鲜时不会）。
  默认配置（每 zone 2 台、区容量 5000）下不可达。要修需让 login 知道单台上限，按水位剔除候选、全部超水位时回落全集；
  不要用「只在较低一半里二选一」，它在 n≤4 时等于回到旧规则；
- 新鲜度过滤要用「数据本身多久没变」而不是镜像刷新时刻：全量重同步会把所有 gate 的刷新时刻重置，死 gate 永远显得新鲜；
  但直接按「最久没变的那台」判镜像过期，又会让一台死 gate 把全部读路径推回直读 etcd。两者要分开设计；
- `proto/login/login.proto` 服务注释仍写 “picks the least-loaded Gate”，生成物同样过时；等下次本来要重生 proto 时一并改。

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
