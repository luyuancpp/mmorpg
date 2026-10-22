# Redis Async Client Retry Fix

Updated: 2026-04-21

## Scope

This document records the retry and reconnect behavior in [redis_client.h](../../cpp/libs/engine/infra/storage/redis_client/redis_client.h) and the scene-side wiring in [redis.cpp](../../cpp/libs/services/scene/core/system/redis.cpp).

## Problems Found

1. Periodic retry logic and reconnect retry logic were sharing the same entry point.
   The old flow could clear `loading_queue_` on a timer, re-issue in-flight GETs, and allow duplicate `load_callback_` delivery for the same key.
2. NIL retries were too aggressive.
   A fixed 2 second interval with only 5 retries gave about 10 seconds of total retry budget, which was not enough for the Kafka -> db service -> Redis write-back path under load.
3. Save operations were dropped on disconnect.
   That made Redis cache recovery depend entirely on later writes from db service.
4. Save retries re-serialized protobuf payloads and used `EVAL` every time.
   That was safe, but not efficient.

## Fix Applied

- Split the retry path into two entry points.
  - `OnReconnected()` is only used by the reconnect callback. It can safely move in-flight loads into the retry queue and force-flush all pending work.
  - `RetryDuePending()` is used by the periodic timer. It only drains due entries from the pending queues and never touches `loading_queue_`.
- Added per-element exponential backoff.
  - Load retries use 2, 4, 8, 16, 32, 60 second intervals.
  - `kMaxLoadRetries` was increased to 6.
  - Each element tracks `next_retry_at` with `steady_clock`.
- Added a save retry queue.
  - `Save()` now serializes once into `Element::serialized_payload` and enqueues on disconnect.
  - Save retries use the same backoff ladder as loads, with `kMaxSaveRetries = 6`.
  - Latest value per key wins because the pending queue is keyed by Redis key.
- Added Lua script caching.
  - The save path now prefers `SCRIPT LOAD` + `EVALSHA`.
  - If Redis replies with `NOSCRIPT`, the code clears the cached sha, reloads the script, and falls back to `EVAL` without consuming a retry slot.
- Reduced logging noise.
  - `AsyncLoad()` debug state is logged at `LOG_DEBUG` instead of `LOG_INFO`.
  - Save disconnects now log at `LOG_ERROR`.

## Cold-Start Reconnect Fix

The original bootstrap sequence in `node.cpp` constructed `Hiredis`, called `connect()`, and only then ran `SetupReconnect()` to register the disconnect callback. If Redis was not yet running when scene started, the very first `SYN` failure happened before any callback was registered and was silently dropped, so the reconnect timer never started.

The fix in [redis_manager.cpp](../../cpp/libs/engine/thread_context/redis_manager.cpp):

- New `RedisManager::Connect(loop, addr)` creates the Hiredis instance, registers connect and disconnect callbacks via the new private `InstallCallbacks()` helper, and only then calls `connect()`. Bootstrap in `node.cpp` switched to this single call.
- `SetupReconnect()` is kept as a thin backwards-compatible alias. If it is ever invoked while `connected()` is false, it now schedules a reconnect immediately so cold start still recovers.
- The connect-callback failure path now also calls `ScheduleReconnect()` (previously it only logged a warning, which left the system stuck).
- `ScheduleReconnect()` is now a no-op when a timer is already active, so duplicated connect-failure and disconnect events do not stack timers.
- A local `kRedisOk = 0` constant is used in place of the `REDIS_OK` macro to avoid relying on transitive includes from muduo's hiredis wrapper.

## Defensive Guards in Raw `tlsRedis.GetZoneRedis()->command(...)` Callers

Two best-effort lookups in [player_scene.cpp](../../cpp/libs/services/scene/player/system/player_scene.cpp) (leader location and team info on enter scene) called `tlsRedis.GetZoneRedis()->command(...)` without null or `connected()` checks. During `DoReconnect()` the unique_ptr is briefly reset, and any business call between the disconnect event and the new connection's `connected = true` would dereference a null pointer or fail silently. Both call sites now guard with `if (!zoneRedis || !zoneRedis->connected())` and return early with a `LOG_WARN`. The two etcd-side callers in `etcd_service.cpp` and `etcd_manager.cpp` already used the correct guard pattern.

## Eager-Serialize Caller Contract

`MessageAsyncClient::Save()` now serializes the protobuf payload into `Element::serialized_payload` at call time so the retry path does not have to re-serialize. The hidden contract: **callers must not mutate the message after passing it to `Save()`** — any post-Save mutation is invisible to the Redis blob.

`PlayerLifecycleSystem::SavePlayerToRedis` previously set `player_id` on the `player_database_data` and `player_database_1_data` sub-tables _after_ calling `Save()` (those fields were used by the Kafka DB-task path immediately following). After the eager-serialize switch, the Redis blob lost those sub-table primary keys. The fix moves `set_player_id` to before `Save()`. The Kafka path still sees the populated values because the mutation is in-place on the same shared message.

## Observability and Lifecycle Hardening

`MessageAsyncClient` now exposes `in_flight_load_count()`, `pending_load_count()`, `pending_save_count()`, and a one-shot `LogQueueSnapshot(tag)` helper. The snapshot logs nothing when all three queues are empty, so it is safe to call from a periodic timer. `RedisSystem::Initialize` runs it every 30 seconds — if the scene cannot drain pending loads or saves, the operator now sees rising counts in the log without enabling per-call DEBUG output.

`RedisManager` is now non-copyable / non-movable and has a destructor that cancels the reconnect timer when the EventLoop pointer is still set. This prevents a stray `DoReconnect()` from firing against a destroyed `this` if the thread-local `tlsRedis` is torn down while the loop is still alive.

## Operational Note

- Windows native Redis service: actual runtime target
- Docker Redis container: idle, empty database

This can mislead diagnosis because the container can look healthy while the scene node is actually talking to the Windows service. Only keep one Redis listener on 6379 during local debugging.

## Verification

All modified files compile cleanly (no errors reported by the language server after each change set).

## 2026-10-20：握手截止与 socket 同批事件的生命周期修复

本节记录当前 `RedisManager` 的契约；上面的冷启动接口和验证说明保留为历史记录。

Linux 的 timerfd 与 Hiredis socket 会进入同一批 `activeChannels_`。旧代码从重连定时器直接执行
`DoReconnect()`，可能先释放握手连接，再分发同批中仍引用它的 socket Channel。Debug 构建会在
`EventLoop::removeChannel` 断言中止，Release 则存在释放后使用风险。只有一个 loop 线程也能触发。

重连定时器现在只投递重建任务，连接的释放和重建统一在 `queueInLoop` 的 pending 阶段执行，
此时本轮 I/O 分发已经结束。定时器与排队任务均先锁定 `LifetimeToken`，失效后不再访问管理器。
每次安排或取消重连都更新代次，排队任务在执行前复核代次和 active 状态：

- 同批 AUTH 成功会作废旧任务，保持刚认证成功的连接。
- 同批建连成功可能先取消旧定时器，再安排新 AUTH 截止；此时 active 又为真，必须由代次区分旧任务。
- Shutdown 或重复 Connect 会作废旧生命周期的任务，不能在之后重新建连。

这是管理器内部的事件阶段约束，不依赖固定延时，也不改变 Hiredis 的独占所有权和业务调用接口。
AUTH 被拒或无应答时仍保持不可用、按原间隔重新认证；同批到达的 AUTH 成功可在 pending 重建前取消旧任务。
这一步只解决管理器定时器重建的分发顺序。当时 Hiredis 自己的连接/命令回调内仍不得同步销毁连接；
下节进一步收口底层所有权，使回调内撤销 owner 和同批其他 Channel 撤销 owner 也能安全清理。

Linux 回归入口为 `cpp/tests/redis_lifecycle_test/CMakeLists.txt`，使用真实 EventLoop、timerfd、
本地假 Redis 和公开管理器接口。专项用例选择 PollPoller 的稳定注册顺序，先确认截止与 socket 同时可读，
再用 loop iteration 确认 timerfd 先于同批 AUTH 分发；无需暴露生产私有接口。
构建保留断言并支持 ASan/UBSan，重新编译 muduo 源码，不复用镜像中带 NDEBUG 的归档。

## 2026-10-21：Hiredis 底层回调与最终释放

### 为什么只有 pending 重连还不够

定时器和 socket 都属于一轮事件分发。Linux `poll/epoll` 先得到一批 `activeChannels_`，再逐个调用
`Channel::handleEvent`。旧 Hiredis 虽然继承 `enable_shared_from_this`，真实调用方却使用 `unique_ptr` 或栈对象；
读、写和关闭回调仍捕获裸 `this`。`removeChannel` 中的 dummy 任务只保住 Channel，没有保住 Hiredis 和 C context。

有两种独立的失败顺序：

1. 同批较早的 timerfd 或其他 Channel 释放 Hiredis，较晚的 socket Channel 仍在待分发列表里。
   Debug 的 `removeChannel` 可能先断言，Release 可能调用已析构对象。
2. Hiredis 自己的连接、命令或断线回调撤掉最后一个业务 owner。`redisAsyncFree` 在
   `REDIS_IN_CALLBACK` 期间只能标记稍后释放；Hiredis 却已析构，hiredis 随后的 `ev.cleanup`
   仍会拿 `ac->ev.data` 回调这个已释放对象。用户回调若正在执行成员 `std::function`，清空它还可能销毁自己的捕获。

这些顺序都不需要两个线程竞争。修复必须覆盖对象、context、Channel、fd 和用户回调捕获的完整寿命。

### 统一创建与所有权

`Hiredis::Create(loop, address)` 是唯一创建入口，构造函数私有。返回 `shared_ptr<Hiredis>`，
RedisManager 和 MessageAsyncClient 的 `HiredisPtr` 同步改为这个类型；示例、直接测试和临时客户端也一起迁移。
`Create(nullptr, address)` 只允许构造永不连接的不可用占位对象，保留旧的未绑定管理器语义。

所有操作、最后一个强引用释放都在所属 loop 线程；EventLoop 必须活过客户端和待销毁任务。
Channel 仍由 Hiredis 管理，外部不取得 muduo `TcpConnection` 的强所有权，不改变 `AGENTS.md §11.7` 的连接所有权规则。

读、写、关闭回调都只捕获 `weak_ptr<Hiredis>`，触发后先 lock；失败直接返回。
Channel 同时 `tie(shared_from_this())`，使一整个 `handleEvent` 中 HUP、READ、WRITE 的组合分发都保住同一对象。
用户在回调内撤掉外部 owner 时，当前分发栈还持有对象，C 库可以安全返回；回调结束后不额外保留业务对象。

### 最后释放必须保住 socket，不能只延迟 Channel

工厂的自定义删除器在 `eventHandling()==true` 时把**整个 Hiredis 的 delete** 投递到本轮 pending 阶段，
其余场合立即删除。投递任务接管最终释放责任；此时原 shared 控制块已经归零，weak 回调不能重新锁回客户端。
所以同批尚未分发的旧 Channel 即使仍在列表中，也只会跳过业务处理。

不能只把 `Channel::remove()` 排队、却立即 `redisAsyncFree`：后者会关闭 fd；较早回调里马上创建的新连接可能复用相同 fd，
而 Poller 还保留旧 Channel 的注册，导致重复注册或错误注销。保住整个对象直到 pending 后再释放，才能同时保住 context 和 fd。
这不是固定毫秒延时，不依赖调度“通常够快”；安全点是本轮全部 I/O 已完成。

真正析构时继续同步调用 `redisAsyncFree`，并保留原有契约：成功登记但尚无应答的命令，以 `reply == nullptr`
恰好回调一次。此时 `me` 仍是析构体内的有效对象，调用方可判定连接状态或尝试新命令；hiredis 的 FREEING 状态拒绝新命令，
拒绝分支立即释放没有登记的回调副本。C hook 在这个同步析构期间直接使用有效的 `ac->ev.data`，不尝试在强引用归零后 lock。

`removeChannel` 仍幂等，并排队保留已注销的 Channel 到当前分发栈退出；Node 仍必须排空 pending 后再 quit，
不能以修复后共享所有权为理由把 EventLoop 提前析构。

### 回调重入与管理器换代

- Hiredis 连接/断线回调以及 RedisManager 重连回调先复制到局部再调用，清空成员不销毁正在执行的捕获。
- 命令回调由局部 `unique_ptr<CommandCallback>` 接管，业务返回后恰好释放一次；不再访问可能被外部撤销的业务 owner。
- 主动 `disconnect()` 自身也保活 wrapper，覆盖 pending 阶段调用 C API 时同步触发断线回调的路径。
- RedisManager 的连接、断线、AUTH、PING 回调先检查弱生命周期令牌。Shutdown 或新 Connect 使旧令牌失效，
  即使旧 Hiredis 的最终释放被排到 pending，旧内部回调也不会访问已销毁或换代的管理器。
- 重连定时器和已排队的重建任务仍检查 `reconnectGeneration_`。同批 AUTH 成功，或建连成功后重新安排 AUTH 截止，
  都会使旧重建失效；共享所有权不能代替业务代次判断。
- 释放业务连接会同步触发用户的空应答回调，用户可能在这里销毁管理器或再次 Connect。
  `Connect` 先把旧连接移到局部，初始化新周期后才释放旧连接；`Shutdown` 先清完全部状态，再释放局部旧连接。
  最后一次业务释放后不再访问管理器成员。存活超时摘除在释放后额外用弱令牌确认原周期仍存在，才安排重连；
  不能只在 pending 任务入口检查一次令牌，就假定用户回调返回后管理器仍然活着。
- `ping()` 不再绑定裸 `this`，并处理空应答，避免断线释放时解引用空 reply。

认证仍然 fail-closed：非空口令只在 AUTH 成功后移交；连接释放时不重发、不伪造业务成功，不改变业务重试或资产提交语义。
本节保证适配器与管理器自身的生命周期，业务传入的 CommandCallback 若捕获其他对象，仍由业务用 weak 或稳定 ID 管理。

### 回归与平台覆盖

Windows 与 `third_party/muduo` 的 Hiredis 公共头及实现同步；Linux 的持久补丁在
`tools/archived/muduo_linux_overlay/contrib/hiredis/`，包括新的工厂头与示例，不手改 gitlink 中的上游。
构造契约改变后库和所有使用方必须配套重编，不能拿旧 Hiredis 布局的库链接新头。

`cpp/tests/rpc_controller_test/hiredis_command_lifecycle_test.cpp` 保留命令格式拒绝、FREEING、DISCONNECTING 和正常释放测试，
并增加连接/应答/断线回调内撤掉 owner 的测试。`cpp/tests/redis_lifecycle_test/hiredis_event_batch_test.cpp`
使用 loopback 假服务端、真实 PollPoller 和 socket：在同批较早 Channel 中释放已可读客户端并马上创建替代连接，
验证旧应答不再分发、旧 fd 在分发结束前仍占用、pending 恰好空回复一次、捕获最终释放、新连接还能完成命令。
原有管理器握手同批测试继续覆盖截止与 AUTH 同轮、Shutdown、新 Connect、迟到代次等路径。

独立入口 `cpp/tests/redis_lifecycle_test/CMakeLists.txt` 必须 Debug、保留断言、默认启用 ASan/UBSan，
当前 overlay 源码重新编译。测试使用本地假服务端，不读取真实 Redis 凭据，不修改系统时钟。
具体执行结果、源码绑定和未验证项见下面的验收记录，不能沿用文首旧的语言服务检查当作运行证明。

### 2026-10-21 实际验收

修复前，真实假服务端上的 `ReplyCallbackCanReleaseLastOwnerAndCompletesPendingCommandOnce`
在回调内撤掉 owner 后命中 `~Hiredis` 的 Channel 断言，退出 134；日志保留在 `hiredis-red-test.log`。
与同目录的三条定时器红态一起，证明问题不是仅凭静态推断。

| 验证范围 | 结果 |
|---|---|
| Windows：Hiredis 命令/回调生命周期、RedisManager 认证/重连/存活及重入 | 27/27，零失败、禁用、跳过 |
| Linux：相同 27 项，加 3 项管理器 timerfd 同批测试与 1 项底层 socket 同批测试 | 31/31，零失败、禁用、跳过 |
| Linux 底层 socket 同批撤销 owner、立即建替代连接 | 连续 20 轮通过 |
| `redis_client.cpp` 与迁移后的 `currency_test.cpp` 消费方编译 | 独立 Windows Debug 对象编译通过，原有裸指针成员检查通过 |

Linux 全部启用 ASan/UBSan 和泄漏检查，最终运行没有报告错误。Windows 库与测试使用本轮源码独立编译，
不与其他聊天的 `game.sln` 产物共用输出。两端完整专项连同定时器合计 Windows **58 项**、Linux **61 项**。
消费方验证只代表编译，**没有运行 currency 真 Redis/资产用例**，也没有运行整个 RPC 工程或重编全部节点。

原件目录 `build/verification/timer-hiredis-complete-20261020/` 包含：
`windows-redis.xml`、`linux-redis.xml`、`linux-batch-repeat.log`、`windows-consumers-final-build.log`、各构建/退出记录、
源码哈希和 `result.json`。Windows 的实际独立工程源在该目录的 `windows-source/CMakeLists.txt`；
Linux 的永久回归入口是本节前述 CMake 工程，运行其 `redis_lifecycle_test` 即可重现完整专项。

本轮还遇到并修复测试工程的增量构建陷阱：`file(COPY)` 保留源时间戳，已覆盖的新 Hiredis 内容可能比旧 `.o` 更早，
链接因此报找不到 `Hiredis::Create`。现在基线与覆盖件用 `configure_file(COPYONLY)` 按内容更新，不重复覆盖仍在使用的 overlay；
删除覆盖件时还原基线或清除仅覆盖件的残留文件。首次链接失败日志保留，最终构建重新编译了 Hiredis 后退出 0。
另保留 Windows 独立消费方工程首次缺 include 的失败原件，补齐与仓库工程一致的 include 后再编译通过，未绕过检查。

三条真 Redis 资产测试的旧“故意泄漏连接”改为明确的退出顺序：用局部 RAII 守卫在业务客户端及其回调捕获还活着时先释放连接，
失败或超时提前返回也执行。不能把底层 Hiredis 自身保活扩大成“所有业务回调捕获都自动安全”。

本轮没有修改真实 Redis 数据、读取业务凭据、调整系统时间或部署节点；没有提交或推送。
现有运行进程仍需按正常构建/发布流程替换，不能用本轮专项通过代替运行产物已经更新的证明。

### 本轮验证回执

- 修复前，首条同批 AUTH 成功回归在 Debug 的 `EventLoop::removeChannel` 断言中止，进程退出 134；
  日志先出现 timerfd，再出现重建及 Hiredis 析构，同批 socket 尚未分发。没有关闭断言来绕过问题。
- 修复后同一用例退出 0；最终完整工程 22/22 通过、0 失败、0 跳过：新生命周期 3 项、命令释放 4 项、
  AUTH/重连 10 项、存活探测 5 项。新用例覆盖同批 AUTH 成功、管理器先析构后旧任务失效、建连成功重挂 AUTH 截止后旧代次失效。
- 新生命周期 3 项再重复 20 轮，stdout 实记 60/60 通过、进程退出 0；重复运行的 XML 只记录最后一轮，不能把它当成 60 项 XML。
- 使用固定 Linux SDK 镜像 `fc6fad59c914...`，串行构建；42 条编译命令均包含 `-UNDEBUG`、
  `-fsanitize=address,undefined`。ASan/UBSan 和泄漏检查未报错。外部 hiredis 1.2.0 使用 SDK 的预编译库，未对它自身重编 sanitizer。
- 最终受测的 6 份源码/构建入口前后 SHA256 全部相同。原始失败、成功 XML、日志、编译命令及
  汇总保存在 `build/verification/hiredis-handshake-lifetime-20261021/summary.json` 同目录。
- Windows 构建因其它 MSBuild/cl 正在运行而未启动；本轮没有重建或重启运行容器，没有修改防火墙、
  端口、TLS、密码或权限。Java 版没有对应 muduo 适配层，本次未改变客户端契约，无 Java 对齐代码或构建。

在准备好固定 SDK 的 Linux 环境中，回归入口如下；`MMORPG_SOURCE_ROOT` 是本轮源码，
`MMORPG_SDK_ROOT` 是已锁定并安装依赖的 SDK 源码根目录：

```sh
cmake -S cpp/tests/redis_lifecycle_test -B build/redis-lifecycle \
  -DCMAKE_BUILD_TYPE=Debug -DMMORPG_SOURCE_ROOT="$PWD" -DMMORPG_SDK_ROOT="$PWD"
cmake --build build/redis-lifecycle --parallel 1 --target redis_lifecycle_test
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  timeout 60s build/redis-lifecycle/redis_lifecycle_test
```
