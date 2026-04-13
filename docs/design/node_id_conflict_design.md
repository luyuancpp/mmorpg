# Node ID 冲突处理设计（重要架构决策）

## 核心场景

**旧节点网络故障重连场景（最危险）：**

```
T=0    节点A(node_id=5, SceneNode) 运行中，有玩家在线
T=30   节点A 网线断开，无法发 keepalive
T=60   etcd TTL 超时，自动删除 /node/5 key
T=61   节点B 通过 Watch 发现 key 消失 → CAS 成功 → 也是 node_id=5
T=65   节点A 健康检查发现 elapsed>TTL → LOG_FATAL → 理论上应当自杀

但存在窗口：T=61～T=65，A 和 B 同时持有 node_id=5
更危险：节点A 网线在 T=63 重新插上 → A 没有触发 LOG_FATAL → A 和 B 长期共存同一 node_id
```

## 当前已有防护（3层）
1. CAS PutIfAbsent - 防止两个健康进程同时写同一 key
2. keepalive TTL=0 → LOG_FATAL 自杀
3. health_check_interval 定时检查 elapsed>TTL → LOG_FATAL 自杀

## 未覆盖的缺口
- 节点A 在 health_check_interval(5s) 检查到期之前已经重连
- 节点A 恢复网络后，其 etcd watch 重连，看到 node_id=5 已被 B 占用
- **此时节点A 没有任何处理逻辑，会继续服务玩家 → 双活 + SnowFlake 冲突**

## 待实现的处理逻辑（用户要求）

### 当节点A 检测到自己的 node_id 被新节点占用时：
1. **立即停止接受新的游戏逻辑请求**（不再处理任何业务消息）
2. **保存所有在线玩家数据**（flush 到 DB/Redis）
3. **踢出所有玩家**，将玩家重定向到同 zone_id 下同类型的另一个节点
4. **自杀退出**

### 副本节点（InstanceNode）特殊情况：
- 副本有状态，不能简单重定向到另一个副本（副本进度唯一）
- 处理策略待定：方案一：直接踢出玩家（副本失败），通知客户端重试
- 方案二：副本状态序列化后迁移（过于复杂，暂不考虑）
- **当前结论：副本节点重连发现 ID 冲突时，直接踢出玩家（副本丢失），告知客户端重进**

## 检测时机
- etcd Watch 事件：收到 PUT 事件且 key 是自己的 node_id key，但 uuid 与自己不同
- 即 `HandlePutEvent` 中检测：key == 自己的 nodeKey && value.node_uuid != 自己的 uuid

## 相关代码位置
- `cpp/libs/engine/core/node/system/node/node_connector.cpp:51` — 已有 uuid 不同时的警告日志，需要在这里或 service_discovery 层加转移逻辑
- `cpp/libs/engine/core/node/system/etcd/etcd_service.cpp:HandlePutEvent` — watch 事件入口
- `cpp/libs/engine/core/node/system/node/node.cpp:HandleServiceNodeStop` — 节点下线处理
- `IsZoneSingletonNodeType` — singleton 节点不受此影响（ForcePut 覆盖语义）

## SnowFlake guard（Redis 防时间窗口冲突）
- 每次 keepalive 写 `SETEX snowflake_guard:{zone_id}:{node_type}:{node_id} 600 {now_seconds}`
- 新节点 CAS 成功后读 guard，**不延迟启动**，而是 `SetGuardTime(now_utc)` 跳过当前秒
- 用 `now`（新节点当前时间）而非 `lastTs`（旧节点写入时间），因为 lastTs 在过去，SetGuardTime 无效
- `now` 已经 > lease 过期时间点，旧节点此时是网络隔离的（lease 过期 = 网络不通），它产生的 ID 出不去
- Guard TTL 固定 600 秒（10 分钟），足够覆盖任何重启场景，不影响逻辑正确性
- 节点秒级启动，无卡顿

### 关键设计决策（用户强调）
**节点重启必须极快**。灰度更新/网线重连场景，玩家体验应像"电线重连"，服务器维护时间尽可能短。
任何导致启动延迟的方案（如等待 safety window）都不可接受。

## OnNodeIdConflictShutdown 虚函数接口
- `Node::OnNodeIdConflictShutdown(NodeIdConflictReason reason)` — 在 LOG_FATAL 之前调用
- 3 个调用点:
  1. `etcd_service.cpp:OnKeepAliveResponse` — TTL=0（kLeaseExpiredByEtcd）
  2. `etcd_service.cpp:OnTxnFailed` — 重注册 CAS 失败（kReRegistrationFailed）
  3. `node.cpp:StartNodeRegistrationHealthMonitor` — 本地检测 lease 超时（kLeaseDeadlineExceeded）
- 基类默认 no-op + 日志。子类 override 实现:
  - SceneNode: 存档玩家 → 迁移到新节点
  - GateNode: 断开所有客户端连接
  - InstanceNode: 通知玩家副本异常终止（副本丢失不可恢复，不同于普通节点迁移）
- 状态：接口已就位，子类 override 待实现

## 灰度更新场景
- 旧节点收到 SIGTERM 时应主动：1) 存档玩家 2) 踢出玩家 3) 注销 etcd key → 新节点无缝接管
- 这个流程目前未实现，需要在 graceful shutdown 中加入
