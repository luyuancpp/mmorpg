# muduo 定时器取消语义与野引用

## 一句话

`EventLoop::cancel()` **拦不住一个已经进入本轮到期批次的定时器**。如果取消的原因是"宿主对象要销毁了"，那么轮到它时回调会打进已释放内存。

## 根因

`TimerQueue::handleRead()` 的结构（上游原样）：

```cpp
std::vector<Entry> expired = getExpired(now);   // ① 先把整批拷出来

callingExpiredTimers_ = true;
cancelingTimers_.clear();
for (const Entry& it : expired)
{
  it.second->run();                             // ② 挨个执行
}
callingExpiredTimers_ = false;

reset(expired, now);                            // ③ 一次性/已取消的在这里 delete
```

① 之后这批定时器已经从 `activeTimers_` 里摘走了。此时若在 ② 的某个回调里
`cancel()` 掉同批次里靠后的另一个定时器，`cancelInLoop()` 会走到：

```cpp
ActiveTimerSet::iterator it = activeTimers_.find(timer);
if (it != activeTimers_.end()) { ... }          // 找不到
else if (callingExpiredTimers_)
{
  cancelingTimers_.insert(timer);               // 只是记一笔
}
```

它**只能记一笔**。上游对 `cancelingTimers_` 的唯一用途在 ③ ——阻止一个
**重复定时器**被重新插回队列。它完全不阻止 ② 里那次 `run()`。

## 崩溃是怎么发生的

1. 实体 A、B 的定时器落在同一批 `expired` 里，A 在前
2. A 的回调销毁了 B —— 移除组件、销毁实体、关闭会话，都算
3. B 的析构里 `Cancel()`，但如上所述拦不住
4. 循环走到 B 的定时器，`it.second->run()` 调用那个捕获了裸 `this` 的
   `std::function` —— **`this` 已经是已释放内存**

这就是当年在 Windows 上撞到的崩溃。

## 修法：只能在 muduo 里拦

`cpp/libs/engine/muduo_windows/src/muduo/net/TimerQueue.cc:193`（提交 `e137bd658`）：

```cpp
for (const Entry& it : expired)
{
    if (cancelingTimers_.contains(ActiveTimer{ it.second, it.second->sequence() }))
    {
        continue;                               // 本轮已被取消，别跑
    }
  it.second->run();
}
```

**必须在这一层。** 组件层的任何"我过期了就返回"式守卫都救不了这个场景：宿主对象
已经被释放，回调为了读那个守卫标志，本身就得先解引用已释放内存 —— UAF 已经发生了。
组件层的守卫只能覆盖"对象还活着但定时器被重设/取消"，覆盖不了"对象没了"。

## 各 muduo 树的现状

仓库里有四份 muduo，守卫只在其中一份：

| 树 | 有守卫 | 谁在用 |
|---|---|---|
| `cpp/libs/engine/muduo_windows/src` | ✅ | Windows 编译进 `muduo.lib` 的实现 |
| `third_party/muduo` | ❌ | Windows 的头文件来源 |
| `third_party/muduo-linux` | ❌ | **Linux 全部** |
| `third_party/grpc/third_party/...` | — | 无关 |

*（2026-10-17 批注：上表与下一句是写作时的现状，已过时。Linux 的守卫已于 2026-08-02 以
`tools/archived/muduo_linux_overlay/muduo/net/TimerQueue.cc` 的 LOCAL DELTA (1) 补上，由
`setup_dependencies.sh` 覆盖到 `third_party/muduo-linux`；`third_party/muduo` 只作头文件来源，守卫在 `.cc` 里，与它无关。）*

**所以 Linux 侧目前没有这个保护**，同一个崩溃在 Linux 节点上是敞开的。
`third_party/muduo-linux` 是子模块，只有指针会被提交，直接改它的工作区活不过一次
clone —— 要补必须走 `tools/archived/muduo_linux_overlay/`（见
`tools/archived/setup_dependencies.sh` 里的 overlay 段落）。

## 相关但不同的一个坑：同批次「重设」

同一批次里，前面的回调**重新调度**了后面那个实体的定时器（而不是销毁它）：

- 旧定时器仍会触发
- 此时宿主还活着，`timerId` 已经换成新的那个
- 若回调无条件把 `timerId` 清空，就会丢掉**新**定时器的唯一句柄
- 宿主销毁时 `Cancel()` 无从取消 → 新定时器活过了宿主 → 到点野引用

这一个组件层能修，也**应该**在组件层修（宿主还活着，读自己的成员是安全的）。
`TimerTaskComp` 用一个每次布防都递增的 `generation` 解决：调度时把当时的代际号绑进
回调，触发时对不上就直接返回。

两个坑要一起堵才完整：

| 场景 | 宿主状态 | 谁来修 |
|---|---|---|
| 同批次被取消 + 宿主销毁 | 已释放 | **muduo 守卫**（组件层无能为力） |
| 同批次被取消/重设，宿主还在 | 存活 | `TimerTaskComp::generation` |

## 另一处两端不一致：定时器精度差一个数量级

> 2026-10-17 订正:本节描述的是旧状态。Windows 的 `EventLoop::loop()` 早已改用 `TimerQueue::nextTimeoutMs` 计算 poll 超时(最近一个定时器的剩余时间,上限 100ms),
> 第 3 批又修好了 Windows 的 `wakeup()`,短定时器不再被 100ms 粒度拖住;定时器到期判断也已改为单调时钟(见上文「定时器时钟」一节)。下文保留作历史。

`cpp/libs/engine/muduo_windows/src/muduo/net/EventLoop.cc`：

```cpp
const int kPollTimeMs = 10000;   // 第 32 行，Linux
const int kPollTimeMs = 100;     // 第 36 行，Windows
```

```cpp
pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
...
#ifdef WIN32
	timerQueue_->loop();          // 每轮 poll 之后才主动查一次定时器
#endif
```

Linux 上 timerfd 是 poller 里的一个真实 fd，poll 精确在到期时刻返回，定时器是
**毫秒级**的。Windows 上没有 timerfd，定时器不由 poll 唤醒，而是每轮循环结束后
查一次，而每轮 poll 最多阻塞 100ms。

**结论：Windows 上任何短于 100ms 的定时器，实际粒度都是 ~100ms。**

实测（`timer_queue_unit_test`，2026-07-31）：

| 用例 | 行为 | 实测 |
|---|---|---|
| `RunEvery(0.01)` 触发 3 次 | 期望 ~30ms | **326ms**（≈108ms/次） |
| `RunAfter(0.01)` 自重设，共 2 次 | 期望 ~20ms | **215ms**（≈107ms/次） |

对战斗服的影响是实打实的：同一个 `RunEvery(0.01)` 的战斗 tick，Linux 10ms、
Windows ~108ms。技能吟唱 / 后摇 / 引导间隔全部受影响。在 Windows 上调出来的
手感，上 Linux 会完全不同；反过来，某些因为 Windows 定时器粗糙而没能暴露的
竞态，到 Linux 上定时器精确触发时才会现形。

**写测试时的直接后果**：不要用固定墙上时间窗口去卡触发次数。窗口若短于一个
poll 周期，在 Windows 上会拿到"定时器只触发了一次"的假象。改成在回调里数够次数
就 `loop.quit()`，另配一个明显偏大的 `runAfter` 兜底。本文件的两条重复/自重设
用例最初就栽在这上面。

## 定时器时钟：改为单调时钟（2026-10-16，未编译）

**问题。** 上游 muduo 的 `TimerQueue` 按墙钟到期：到期判断、上弦时长、重复定时器的下一次
到期全用 `Timestamp::now()`（Windows 上是 `gettimeofday` → `system_clock`）。Linux 的 timerfd
虽然建成 `CLOCK_MONOTONIC`，上弦时长仍按墙钟差算。墙钟回拨 N 秒（NTP 步进、人工改时间），
所有定时器——调用截止、建连、探测、业务看门狗——都晚 N 秒；前跳则提前。业务侧"到点按
steady_clock 复核、未到就重挂"只兜得住前跳，兜不住回拨（`grpc-muduo-transport.md` §6 登记过的已知限制）。

**做法（与 POSIX timerfd `CLOCK_MONOTONIC`、`asio::steady_timer` 同一口径）。**

- `TimerQueue::monotonicNow()`：`steady_clock` 的微秒读数，借 `Timestamp` 承载（复用 `addTime` /
  `operator<` 与 `Timer` 字段）。**它不是 epoch 时间**：原点约为开机时刻，只能与同一时钟的读数
  相减、比较，不能格式化成日期、不能与 `Timestamp::now()` 混算。
- 队列里 `Timer::expiration()` 一律在单调域；`handleRead` / `getExpired` / `reset` / `Timer::restart`、
  Windows 的 `nextTimeoutMs`、Linux 的 `howMuchTimeFromNow` 全部取单调 now。
- `EventLoop::runAfter` / `runEvery`：单调 now 加时长。
- `EventLoop::runAt(墙钟时刻)`：调用时一次性换算，`monoWhen = monoNow + (wallTime − wallNow)`
  （`TimerQueue::monotonicFromWall`，纯函数）。公开 API 不变；语义变为"等价于
  `runAfter(time − now)`"，之后墙钟再跳也不跟随。需要"墙钟到某一刻才触发"的整点类任务，
  到点后自己按墙钟复核。现在生产代码没有 `runAt` 调用方（只有 `TimerTaskComp::RunAt` 转发与测试）。
  两只表显式先读墙钟、后读单调钟（不靠实参求值顺序）：两次读数之间被抢占时，截止时刻只会偏晚、不会偏早。
- `reset()` 不再用 `nextExpire.valid()`（> 0）判断"还有没有下一个"：由很久以前的墙钟时刻换算来的
  单调值可以 ≤ 0，改为直接看队列是否为空。
- 墙钟只用于对外显示：`pollReturnTime()`、日志时间戳不变。TRACE 日志里 `TimerQueue::handleRead()`
  打印的 now 改标 `at monotonic`。

**各树落点。**

| 树 | 改动 |
|---|---|
| `cpp/libs/engine/muduo_windows/src` | `TimerQueue.{h,cc}`、`EventLoop.cc`；`EventLoop.h` / `Timer.h` 只加注释 |
| `third_party/muduo`（Windows 使用方 include 的头副本） | `TimerQueue.h` / `EventLoop.h` / `Timer.h` 与上一行逐字一致（`TimerQueue.h` 顺带补齐原先缺的 `nextTimeoutMs` 声明） |
| `tools/archived/muduo_linux_overlay` | `TimerQueue.cc`（LOCAL DELTA (2)）、新增 `TimerQueue.h` / `EventLoop.cc` 覆盖件；三者必须一起生效 |

类布局都不变（只加静态成员函数），`EventLoop.h` 公开 API 不变。

**业务侧的 steady 复核保留。** `muduo_grpc` 的截止 / 建连 / 探测 / 排空定时器、`RunAtMonotonic`
（跨区换图看门狗）、agones 等待者都另记一份 `steady_clock` 截止点，到点复核、未到就按剩余时间重挂。
改为单调时钟后它们不再需要兜墙钟跳变，但仍兜得住"秒数转微秒的截断让定时器早到一点"，且无害，
所以保留，只把注释改准。

**测试。** `timer_queue_unit_test` 的 `TimerClockTest.*`：换算纯函数的偏移与保序、`monotonicNow`
不回退、过去墙钟时刻乱序挂仍按墙钟先后触发、1970 年的墙钟时刻（单调域为负）照常触发且后续定时器
照常上弦、`runAt` 与 `runAfter` 同域比较、Windows `nextTimeoutMs` 按单调时钟倒数。不改系统时钟；
"墙钟回拨后定时器不再晚到"的端到端效果需在可改系统时间的环境里手工验证。

## 写定时器的规约

- **单例 / `thread_local` 对象**：捕获列表留空，回调里用全局句柄取。
  `cpp/libs/services/scene/core/system/redis.cpp:89` 就是这个写法。
  这样"生命周期"这个问题从存在变成不存在，不需要论证。
- **per-instance 对象**（每实体、每连接、每会话）：走 `TimerTaskComp`，
  不要裸捕获 `this` 去挂 `loop->runAfter`。它析构自动 `Cancel()`，
  有代际校验，两个平台行为一致。
- **回调里要碰的对象可能被销毁**：捕获**稳定 ID**（entity id / player id）
  而不是指针，触发时再查一次，查不到就返回。这条同时免疫"对象被移动"。

## 相关

- 回归测试在 `cpp/tests/timer_queue_unit_test/timer_queue_unit_test.cpp`，两条：

  - `CancelledTimerInSameBatchMustNotRun` —— 最小语义：同批次被 `cancel()`
    的定时器不得执行。
  - `TimerOfObjectDestroyedInSameBatchMustNotRun` —— **本文描述的那次崩溃的
    原形**：宿主对象在同一批次里先被销毁（析构里规规矩矩 `cancel()` 了），
    定时器随后到点。

  两条都用**裸 muduo 定时器**而不是 `TimerTaskComp` —— 后者 `Cancel()` 会顺手
  清掉 `callback`，那样即使 muduo 真的跑了这次触发也看不出来，测不到守卫在不在。

  第二条里的回调**故意不解引用 `this`**，只给一个外部计数器加一。真实代码当然
  会碰 `this`，那正是崩溃点；但测试若也去碰，它自己就成了未定义行为，而不是
  一个能稳定判定的探测器。计数器活在测试栈上、lambda 活在尚未销毁的 Timer 里，
  所以"回调跑了没有"这个观测本身是良定义的。
- `TimerTaskComp` 作为 ECS 组件成员时，还有一个独立的坑（entt 删除组件走
  swap-and-pop 会移动元素，`TimerTaskComp` 的移动赋值会把源也 `Cancel()`，
  导致误杀另一个实体的定时器）。属于 entt 存储策略问题，不在本文范围。
