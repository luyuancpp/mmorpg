# SLG Server Architecture Design (率土之滨 Style)

**Created:** 2026-03-30

## 1. 寻路 (Pathfinding)

**地图模型**：格子地图（hex 或 square），不是 NavMesh。

**算法选择**：
- **A\*** 足够。格子数量级 ~10^6，A* 在有良好启发函数时性能完全够用
- 对于长距离寻路，使用 **JPS（Jump Point Search）** 可以在方格图上获得 10-30x 加速
- 如果地图有大面积可通行区域，可预计算 **HPA\*（Hierarchical Pathfinding）**：将地图分为 chunk，先做粗粒度跨 chunk 寻路，再做 chunk 内精细寻路

**服务端实现要点**：
- 寻路在服务端完成（防作弊），客户端只做预测展示
- 地图障碍物变化（建城、拆除）时，只需更新受影响 chunk 的局部连通图
- 路径缓存：相同起终点 + 相同地图版本号 → 命中缓存直接返回
- 行军路径一旦计算完成，存储为 `Vec<TileId>` 序列，服务端按 tick 推进位置

```
March {
    path: [tile_0, tile_1, ..., tile_n],
    current_index: 3,
    speed: tiles_per_second,
    start_time: timestamp,
    // 当前位置 = path[floor((now - start_time) * speed)]
}
```

---

## 2. 战斗 (Combat)

率土之滨的战斗是 **回合制自动战斗**，不同于即时战斗 MMORPG。

**架构**：
- 战斗发生在两支（或多支）部队到达同一格子时触发
- 战斗逻辑是**确定性模拟**（Deterministic Simulation）：相同输入 → 相同结果
- 服务端运行完整战斗模拟，生成**战报（BattleReport）**发给客户端回放

**实现要点**：
- 战斗系统是纯函数：`BattleReport = SimulateBattle(attacker_army, defender_army, random_seed)`
- 不需要实时同步，客户端拿到战报后本地播放动画
- 使用固定 seed 的伪随机数，保证战报可重放验证（防外挂/争议回溯）
- 多队集结攻城时，按到达时间排队，依次或合并处理

**性能**：
- 单场战斗通常 < 1ms（回合数有限，单位数有限）
- 核心瓶颈不在战斗计算，而在并发触发量。用战斗队列 + worker pool 处理突发峰值

---

## 3. 视野 (Vision / Fog of War)

这是 SLG 大地图最核心的系统之一。

**需求**：玩家只能看到自己城池/部队周围一定范围内的信息。

**方案：格子级 interest 计数器**

```
每个格子维护: observer_count map<player_id, int>

当玩家建城/行军经过格子时:
  对视野范围内所有格子: observer_count[player_id]++
当离开时:
  observer_count[player_id]--
  
player 能看到格子 ⟺ observer_count[player_id] > 0
```

**优化**：
- 视野范围通常是固定半径（如 3-5 格），用**预计算的偏移表**（offset table）批量更新
- 行军时，只需做**增量更新**：新进入的前沿格子 +1，离开的尾部格子 -1（类似滑动窗口）
- 不要每 tick 全量重算视野

**大地图分区**：
- 将地图分为 N×N 个 **Region**（如 32×32 格为一个 Region）
- 每个 Region 由一个线程/协程负责，减少锁竞争
- 跨 Region 行军时做 Region 间 handoff

**推送策略**：
- 不要广播全地图变化给所有人
- 每个格子状态变化时，只通知 `observer_count > 0` 的玩家
- 玩家打开大地图滚动时，按视口（viewport）请求格子数据，服务端做**权限过滤**后返回

---

## 4. 行军同步 (March Synchronization)

**核心模型：时间驱动 + 事件驱动混合**

行军不需要每帧 tick，而是基于时间计算的**惰性求值**：

```
current_tile = path[min(floor((now - t_start) * speed), len(path) - 1)]
```

**服务端实现**：
- 行军创建时，计算完整路径和预计到达时间
- 注册一个**定时器事件**：到达目的地时触发（触发战斗/驻扎/采集等）
- 中途不需要 tick 推进位置——任何时刻都能从公式算出当前位置
- 路径上每经过一个格子，注册**视野进入/离开事件**（可预计算所有时间点）

**中途拦截/遭遇**：
- 两支部队在同一格子相遇时需要触发战斗
- 实现方式：行军注册时，在路径经过的每个格子上注册 **到达时间**
- 新行军注册时，检查路径上是否有其他部队同时间段经过同一格子（时空碰撞检测）
- 或者更简单：每个格子维护 `marching_through` 列表，到达时检查冲突

**客户端同步**：
- 客户端收到行军信息：`{path, start_time, speed}`
- 客户端本地插值动画，不需要服务端每帧推送位置
- 加速/召回等操作：服务端重新计算路径和时间，推送更新事件

---

## 5. 性能优化 (Performance)

| 层面 | 策略 |
|------|------|
| **地图分区** | 将大地图切为 Region，每个 Region 独立处理，减少全局锁 |
| **惰性计算** | 行军位置按需计算，不做每帧 tick |
| **增量视野** | 滑动窗口式更新，不要全量重算 |
| **战斗异步化** | 战斗放入队列由 worker pool 处理，不阻塞主逻辑 |
| **AOI 优化** | 玩家只订阅视口内 Region 的变更事件，减少推送量 |
| **数据冷热分离** | 活跃行军/在线玩家数据在内存；离线玩家/历史战报落库 |
| **时间轮定时器** | 行军到达、建筑完成等大量定时事件，用时间轮（Hashed Timing Wheel）而非优先队列 |
| **协议优化** | 大地图格子数据用差量（delta）推送，不要全量 snapshot |

---

## 6. 数量级参考（单服）

- 同时在线 ~5,000–10,000 人
- 同时行军 ~10,000–50,000 支部队
- 地图格子 ~1.5 × 10^6
- 每格子内存 ~64–128 bytes → 地图总内存 ~100–200 MB，完全放得进内存

---

## 7. 与当前 MMORPG 项目的关系

| 差异点 | MMORPG (当前项目) | SLG (率土之滨) |
|--------|-------------------|----------------|
| AOI | 连续坐标九宫格/十字链表 | 格子级 observer 计数 |
| Scene | 多个小 instance | 一张超大共享地图，Region 分片 |
| 同步模型 | 帧同步/状态同步 | 事件驱动 + 惰性位置计算 |
| 战斗 | 实时技能/伤害同步 | 确定性回合模拟 + 战报回放 |
| Kafka 消息 | 可复用 | 行军事件、战斗结算、跨 Region 通知 |

---

## 8. 战斗回放系统 (Battle Replay)

### 8.1 核心原则

确定性模拟 + 事件日志序列化。服务端运行完整战斗，产出 BattleReport（行动序列），客户端从日志回放动画。

### 8.2 确定性引擎约束

- 纯函数：`BattleReport = SimulateBattle(attacker, defender, seed, terrain)`
- 禁止浮点数 — 用定点数或整数百分比（如 `atk * 120 / 100`）
- 禁止依赖系统时间、内存地址、容器遍历顺序等不确定因素
- 随机数统一用一个 seeded PRNG（如 xoshiro256），调用顺序必须固定

### 8.3 BattleReport 数据结构

```protobuf
message BattleReport {
    uint64 battle_id       = 1;
    uint64 random_seed     = 2;
    ArmySnapshot attacker  = 3;
    ArmySnapshot defender  = 4;
    repeated Round rounds  = 5;
    BattleResult result    = 6;
}

message Round {
    uint32 round_number          = 1;
    repeated BattleAction actions = 2;
}

message BattleAction {
    ActionType type        = 1;   // NORMAL_ATTACK, SKILL, BUFF, DEBUFF, HEAL, DEATH
    uint32 source_slot     = 2;   // 发起者位置 (0-5)
    uint32 target_slot     = 3;   // 目标位置
    uint32 skill_id        = 4;   // 触发的战法ID
    int32  damage          = 5;   // 伤害/治疗量
    int32  source_hp_after = 6;   // 行动后血量（自包含，客户端不需要自己算）
    int32  target_hp_after = 7;
    repeated BuffEvent buffs = 8;
    bool   is_critical     = 9;
    bool   is_dodged       = 10;
}

message BuffEvent {
    uint32 buff_id   = 1;
    uint32 target    = 2;
    bool   is_add    = 3;   // true=添加, false=移除
    int32  remaining = 4;   // 剩余回合数
}
```

**设计要点**：
- 每个 Action 自包含 hp_after → 客户端展示永远和服务端一致
- Action 序列线性 → 客户端按顺序逐条播放动画
- 典型大小：~1-5 KB/场（protobuf）

### 8.4 客户端回放流程

```
收到 BattleReport → 进入战斗场景，摆放武将模型
  ↓
for each round:
    for each action:
        播放对应动画 (普攻/技能特效/buff图标/死亡)
        更新血条到 action.target_hp_after
        等待动画结束（快进时跳过）
  ↓
展示结果面板（胜负、战损、经验）
```

**快进/跳过**：1x / 2x / 直接跳到结果。Action 包含状态快照 → 任意位置可 seek。

### 8.5 事件日志 vs 输入重模拟

| | 事件日志（SLG 首选） | 输入重模拟 |
|---|---|---|
| 战报内容 | 完整 Action 序列 | 只存 BattleInput + seed |
| 客户端要求 | 只需动画播放器 | 必须内嵌完整战斗逻辑 |
| 数据量 | ~1-5 KB/场 | ~200 bytes/场 |
| 跨版本兼容 | 旧战报永远可播放 | 逻辑变更后旧 seed 结果不同 |
| 安全性 | 客户端无战斗逻辑可逆向 | 客户端有完整逻辑 |

选择事件日志：长期存储（联盟战史）、跨版本兼容、降低逆向风险。

### 8.6 存储与分发

- 实时推送给双方玩家
- 压缩存储：protobuf serialize → zstd → DB blob（~500B-2KB/条）
- 每人保留最近 100 条 → ~200KB/人
- 全服 10 万玩家 → ~20GB，完全可控
- 超过 30 天归档冷存储
- 历史查看：请求 battle_id → 解压 → 返回

### 8.7 防篡改校验

- CRC32 校验和覆盖 input + 所有 actions（服务端生成）
- 争议时可用 BattleInput 重新模拟，对比 Action 序列
- 更强保证：HMAC 替代 CRC32
