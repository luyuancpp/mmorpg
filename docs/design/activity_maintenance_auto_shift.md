# 活动遇停服自动顺延设计

## 背景
运营活动通常按时间窗发放（例如 7 天签到、限时征战、累计充值）。当出现国家公祭日、紧急维护、机房故障等全服不可用场景时，如果不做顺延，玩家可参与时长会被动缩水，带来不公平和投诉风险。

目标是实现：
- 停服导致的不可用时段不计入活动有效时长。
- 活动顺延规则可配置、可审计、可回放。
- 重算幂等，避免重复顺延导致时间漂移。

## 设计目标
- 正确性：同一组输入数据反复计算，结果完全一致。
- 公平性：玩家可参与时长与预期一致。
- 可运营：支持指定活动不顺延、顺延上限、手工修正。
- 可观测：每次顺延有日志和审计记录。

## 核心思路
不要直接改写活动原始时间。采用两层时间模型：
- 基线时间 Baseline：活动配置的原始 start/end，不变。
- 生效时间 Effective：由 Baseline + 停服窗口计算得出，可重算。

这样即使停服事件重复投递、消息补偿重放，也不会出现累计误差。

## 数据模型建议

### 1) 活动模板（静态）
- activity_id
- baseline_start_ts
- baseline_end_ts
- shift_policy
- max_shift_days
- shift_enabled

### 2) 停服日历（运营侧）
- block_id
- block_start_ts
- block_end_ts
- block_type: maintenance | public_holiday | incident
- affects_activity: bool
- scope: global | zone

### 3) 活动运行态（动态）
- activity_id
- effective_start_ts
- effective_end_ts
- accumulated_shift_seconds
- calc_version
- last_recalc_reason

## 顺延策略

### A. 时长补偿（最常用）
适合绝大多数限时活动：
- 计算停服窗口与活动有效区间的重叠秒数 overlap_seconds。
- effective_end = baseline_end + overlap_seconds。

公式：
- overlap = sum(intersection(activity_window, each_block_window))
- effective_end = baseline_end + overlap

### B. 自然日跳过（按天活动）
适合“活动持续 N 个自然日”：
- 统计可用日，不可用日不计入。
- 遇停服日则自动向后找下一个可用日。

### C. 混合策略
- 先保证最少可用小时数，再受 max_shift_days 限制。
- 防止极端故障导致活动无限延长。

## 计算流程（推荐事件驱动）
1. 运营录入或更新停服窗口（可提前录入国家公祭日）。
2. 产生事件 MaintenanceWindowChanged。
3. 活动调度服务消费事件，筛选受影响活动。
4. 基于 Baseline + 全量有效停服窗口重算 Effective。
5. 使用 CAS/版本号更新运行态，写审计日志。
6. 广播活动变更给网关/场景服/前端缓存。

## 幂等与一致性
- 重算必须全量函数化：
  - 输入 = 基线活动 + 当前有效停服窗口集合。
  - 输出 = 唯一的 effective_start/end。
- 不采用“在旧 effective_end 基础上继续加秒”的增量算法。
- 使用 calc_version（或更新时间戳）防止并发覆盖。

## 关键边界条件
- 多段停服窗口重叠去重。
- 跨午夜停服与跨月活动。
- DST/时区问题：存储统一 UTC，展示再转本地时区。
- 活动已结束后新增停服窗口：
  - 按策略可忽略，或触发补偿邮件而非顺延。
- 指定活动不顺延（例如强绑定节日档期）。

## 运营控制面
建议提供以下开关：
- shift_enabled（活动维度）
- max_shift_days（防无限延期）
- block_affects_activity（停服窗口维度）
- force_recalc API（人工触发重算）

## 审计与观测
每次重算记录：
- activity_id
- before_effective_start/end
- after_effective_start/end
- block_ids
- reason
- operator / source_event_id

监控指标：
- activity_shift_count
- activity_shift_seconds_total
- recalc_fail_count
- recalc_latency_ms

## 测试清单
- 单次整天停服（最常见公祭日）。
- 多次停服叠加。
- 重复事件重放（应幂等）。
- 并发重算冲突（版本保护）。
- 活动末尾停服、活动中段停服、活动开始前停服。
- 跨时区显示一致性。

## 实践建议
率土之滨这类长线运营游戏，通常会选“基线不变 + 生效重算 + 审计可追溯”的模式。这样运营临时插入停服、补录公祭日、撤销误配置都能安全回滚和重放，不会把活动时间越改越乱。