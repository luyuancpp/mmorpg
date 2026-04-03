# Hashed Timing Wheel

George Varghese & Tony Lauck, 1987 论文 "Hashing and Hierarchical Timing Wheels"。

## 核心思想
- 固定大小环形数组（wheel），每个 slot 挂链表存放到期定时器。
- tick 指针按固定间隔前进一格，处理当前 slot 所有到期任务。

```
  slot 0 → [timerA] → [timerB]
  slot 1 → (empty)
  slot 2 → [timerC]
  ...
  slot N-1 → [timerD]
       ↑
     current tick
```

## 复杂度
| 操作 | 复杂度 |
|------|--------|
| 添加 | O(1) — hash 到 slot，链表头插 |
| 取消 | O(1) — 双向链表直接删除 |
| Tick | 均摊 O(1) |

对比：最小堆/红黑树 O(log n)。定时器数量巨大时 timing wheel 常数因子优势明显。

## 大跨度超时的两种经典解法

1. **Hierarchical Timing Wheel**（层级轮）— 类似时钟的 时/分/秒 多层轮盘，高层轮 tick 时把定时器"降级"到低层轮。
2. **Hashed Timing Wheel**（哈希轮）— 单层大轮，用 `expiry % wheel_size` 做 hash，同一个 slot 里可能混合不同"圈"的定时器，tick 时需检查实际到期时间。取名 "hashed" 来自这个取模/哈希映射。

## 实际应用
- **Linux 内核** timer 子系统（层级 timing wheel）
- **Netty** HashedWheelTimer（Java 网络框架最常用定时器实现）
- **Kafka** 内部用 hierarchical timing wheel 管理延迟操作
- 各种网络库连接超时/心跳检测

## 本项目现状
- `cpp/libs/engine/core/time/comp/timer_task_comp.cpp` 使用 EventLoop 级定时器。
- 底层 muduo 风格实现用 `std::set`/红黑树，O(log n)。
- 当前规模够用；单进程管理数十万定时器时才值得切换到 timing wheel。
