# Gate–Scene 连接规模与跨 zone 换 Gate

2026-10-15 按 `6281e4d27` 核对。本文说明已落码的连接拓扑；本次只改文档，未执行构建、联机或容量验证。

## 当前链路

| 通路 | 传输 | 连接边界 |
|---|---|---|
| Gate ↔ Scene 玩家实时消息 | muduo TCP RPC（RPC0） | Gate 只连接本 zone 的 Scene |
| SceneManager / login → Gate 的路由、踢人、会话绑定、重定向 | Kafka 命令 | 共享分区 topic；目标身份由命令路由和实例 UUID 校验 |
| Gate → Go 业务服务 | gRPC；路由模式先到 `client_rpc_router` | 与 Gate↔Scene 的 TCP 通道独立 |
| Scene → SceneManager 换图、镜像创建、疏散 | gRPC | SceneManager 编排场景落点 |

Gate 的两种客户端路由模式都把 `SceneNodeService` 列入出站白名单。服务发现通过 zone 过滤限制
Scene 候选；服务启动后发现可连接节点就调用 `NodeConnector::ConnectToNode`，启动时也会遍历
已有发现记录。TCP 分支创建 `RpcClient` 后立即 `connect()`，不是等玩家首次使用才 lazy 建连。
`grpc_channel_cache.h` 只管理 gRPC channel，不能用来描述 Gate↔Scene 的连接复用。

实现入口：`cpp/nodes/gate/main.cpp`、
`cpp/libs/engine/core/node/system/discovery/service_discovery_manager.cpp`、
`cpp/libs/engine/core/node/system/node/node_connector.cpp`。

## 为什么按 zone 限制连接

若允许所有 Gate 连接所有 Scene，连接对数上界为 `G × S`；按 zone 限制后为
`Σ(G_zone × S_zone)`。这限制的是连接扇出，不消除单 zone 内的连接乘积。

| 假定规模 | 不分 zone 的连接对数 | 各 zone 均匀分配时的连接对数 |
|---|---:|---:|
| 100 Gate、50 Scene、1 zone | 5,000 | 5,000 |
| 1,000 Gate、200 Scene、10 zones | 200,000 | 20,000 |
| 40,000 Gate、2,000 Scene、100 zones | 80,000,000 | 800,000 |

以上只是拓扑算式，不是实测容量、活跃 socket 数或延迟承诺。双向连接及重连期间的实际资源占用
须结合节点建连规则测量。

## 跨 zone：客户端换到目标 zone 的 Gate

```text
客户端 → Gate-A → Scene-A → SceneManager.EnterScene
                           │
                           ├─ 源 Scene 冻结、存盘并写 handoff 标记
                           ├─ SceneManager 校验 owner_epoch / handoff
                           └─ 选择目标 zone Gate，签发重定向凭证
客户端 ← Gate-A ← Kafka RedirectToGate
客户端断开 Gate-A，连接 Gate-B、验证凭证并重新 EnterGame
login / player_locator 更新新会话 → BindSession / RoutePlayer
Gate-B → 本 zone Scene-B（muduo TCP RPC）→ 从玩家数据层加载后落地
```

这是链路示意，完整顺序、换手门、回滚和两条腿的凭证规则以
[cross-zone-scene-travel.md](cross-zone-scene-travel.md) 为准。`RedirectToGate`、zone 发现过滤和
目标 Gate 的重新入场已在本仓实现，不再列为待新增功能。跨 zone 不搬运 `PlayerAllData`，也不让
Gate-A 直接连接远端 Scene-B。

不同 Gate 重连是已有能力：新 Gate 的 SessionInfo 由新连接与 BindSession / RoutePlayer 重建，
`player_locator.Reconnect` 更新 Gate 和 session。当前连接的状态属于原 Gate，不代表重连必须
返回原 Gate。参见 [player_login_flow.md](player_login_flow.md) 的不同 Gate 重连流程。

## 控制面 topic 与 token 边界

命令 topic 按 [control-plane-topic-partitioning-20260908.md](control-plane-topic-partitioning-20260908.md)
计算，例如 `gate-cmd_g<N>`；旧 `gate-{id}` 是迁移窗口的旧记法。生产者须填写目标 node 与
`target_instance_id`，按玩家实体 ID 保序。

Gate Token 仍由 login 签发、Gate 本地验签，并绑定选中的 Gate。客户端重连可以重新选择 Gate，
但已经签给 Gate-A 的票不能被任意负载均衡到 Gate-B。集群外入口使用每实例可寻址的 Gate，见
[gate-load-balancing-design.md](gate-load-balancing-design.md) 与 [k8s-client-entry.md](k8s-client-entry.md)。

## 未采用的中继层

在 Gate 与 Scene 之间增加无状态 Relay 可以改变连接扇出，但会引入额外转发、生命周期和故障
处理。本仓采用 zone 内直连与跨 zone 换 Gate，没有新增 Relay 节点。原方案里的延迟估值不作为
当前系统的测量结论。

## 代码与验证入口

| 文件或文档 | 职责 |
|---|---|
| `go/login/internal/logic/loginpregate/assigngatelogic.go` | AssignGate 的编排入口 |
| `go/login/internal/logic/pkg/loginqueue/gatetoken.go` | 选 Gate 与签票 |
| `go/scene_manager/internal/logic/gate_redirect.go` | 跨 zone 重定向 |
| `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` | token 校验与客户端转发 |
| `cpp/nodes/gate/handler/event/gate_event_handler.cpp` | Kafka 控制事件处理 |
| [cross-zone-scene-travel.md](cross-zone-scene-travel.md) | 已实施链路、剩余缺口和交接索引 |
| [跨区故障注入手册](../ops/cross-zone-failure-test-runbook.md) | 真实环境验收步骤与证据边界 |

工作区正在进行的 muduo RPC channel 改造不改变本文的 TCP/gRPC 平面划分；不能据未提交改造
宣称新回调、超时或完整节点构建已经验证。
