# G3 实现规格:新号首次进场免冷读(交实现阶段)

> 本文件分两部分:
> - **Part A**:设计文档全文。实现阶段把两条 `<!-- BEGIN/END -->` 标记之间的内容原样落成 `docs/design/new-player-first-enter-no-cold-read.md`。
> - **Part B**:实现规格,包括文件清单、逐文件代码、类型 / 脚本 / 配置键、错误语义、测试清单、交 Codex 的验证命令、偏离与代价、风险、待拍板事项、Java 版判断、未核实项。
>
> 本阶段只读代码与文档,除本文件外没有改任何文件。没有编译、测试、gofmt、生成或部署(AGENTS §10.1)。
> 2026-10-12 的对抗式评审已经吸收进正文,逐条处理见文末「评审处理记录」。
> 行号基准是 HEAD `73b852f32`:
> - `go/login/**`、`deploy/login-stack.linux/login.yaml`、C++ 全部相关文件、`go/db/internal/kafka/placement_route.go`、`go/shared/**`、`tools/merge_zone/**` 在工作区与 HEAD 相同。
> - `go/db/internal/kafka/key_ordered_consumer.go`、`tools/scripts/k8s_deploy.ps1`、`docs/design/player-storage-placement.md`、`docs/design/zone_data_rollback.md`、`docs/README.md`、`tools/scripts/gen_docs_index.py` 有他人未提交的改动,按**工作区**行号引用。

---

<!-- BEGIN docs/design/new-player-first-enter-no-cold-read.md -->
# 新号首次进场免冷读:建角时写入 PlayerAllData 种子

> **状态**:提案(开服准备 L-05 / G3),2026-10-12 起草,等负责人按 §10 拍板。代码未落,没有编译、测试或压测;落码后按 AGENTS.md §10.1 交 Codex 验证。
> **依据**:只读核对代码、配置与文档,HEAD `73b852f32`。`go/db/internal/kafka/key_ordered_consumer.go`、`tools/scripts/k8s_deploy.ps1`、`docs/design/player-storage-placement.md`、`docs/design/zone_data_rollback.md` 有他人未提交的改动,按工作区行号引用;其余文件工作区与 HEAD 相同。
> **起因**:[tidb-phase1-and-launch-readiness.md](../../design/tidb-phase1-and-launch-readiness.md) §7 的 L-05(:816)。新号第一次 EnterGame 一定冷读数据库,玩家在加载屏上同步等待,每个读任务的等待上限是 8s。首日这是 Phase 2 全局玩家库唯一的同步路径,也是单 zone 预加载链的第一个瓶颈。
> **关联**:[player-storage-placement.md](../../design/player-storage-placement.md) §13、[player-route-mapping-durability-proposal.md](../../design/player-route-mapping-durability-proposal.md) §12、[cross-zone-scene-travel.md](../../design/cross-zone-scene-travel.md) CZ-2 / CZ-4、[scene-owner-reentry-barrier.md](../../design/scene-owner-reentry-barrier.md)、[no-single-node-horizontal-scaling-20261001.md](../../design/no-single-node-horizontal-scaling-20261001.md) §5。

## 1. 结论

- **做法(方案 A)**:CreatePlayer 在写账号 blob 之前,先往 `PlayerAllData:{id}` 写一份「种子」。
  - 种子的内容和今天冷读读空后 login 组装出的父键完全一样:两张子表各只带 `player_id`。
  - TTL 也一样,都是 24h。
  - 新号第一次 EnterGame 时,预加载命中缓存快路径,不再发 Kafka 读任务,不碰 go/db,也不用等那 8s。
- **为什么正确**:
  1. 号段 id 不复用。刚铸出的 id 在任何库里都没有行,「读到空」本来就是真值,种子只是把这个结论提前写进缓存。
  2. 种子和冷读结果出自同一个构造函数。之后的身份补齐、scene 的新号初始化、首次存盘都走今天的老路,C++ 和 go/db 一行不改。
  3. 写入脚本只在「父键不存在,且 `player:{id}:owner_epoch` 也不存在」时才落地,覆盖不了任何真实存档。
- **种子不是真源,只是缓存优化**。写失败、过期、被删、Redis 丢键,首次进场都会退回今天的冷读,结果相同。
- **开关**:`NewPlayerSeed.Enabled`,默认关。关着时和今天逐字节相同。
- **不选「新号标记」(方案 B)**。它有两种做法:要么退化成「推迟到首次进场才写」的 A;要么让 scene 凭一个可能已过期的标记把空档当真,出错时空档会覆盖真实进度。
- **代价**:
  - 建角时多一次 Redis 往返,落在账号锁内。
  - 未进场角色的种子各占约百字节 Redis,24h 后自动过期。
  - **少了一个早期信号**。`player-storage-placement.md` §13 第 3 条禁止的两种违规配置:go/db 开了 `Placement.Required=true` 而 login 没钉落点;`NewPlayerStorageId` 指向 go/db 打不开的库。今天两者都表现为「新号预加载失败、进不了游戏」;打开种子后变成「能进场,但存盘进死信或无限重试,进度只在 Redis、不进 MySQL」,更隐蔽。打开种子之前两类告警必须先就位(§9、§10)。
  - **新号的入场链改在 PreloadPool 的 worker 上跑完**。快路径在池 worker 里同步跑完整段 apply;冷读路径发完 Kafka 就把 worker 还回去。开种子后,每个新号占用池 worker 的时间变长,池满回 `kLoginInProgress`(§6 第 21 行)。老玩家命中缓存时今天就是这样。
- **Java 版不适用**:Java 建角时同步插入玩家行,进场按主键读本库,没有这条冷读链(§11)。

## 2. 现状链路(逐步核对)

### 2.1 建角:写名字、归属映射和账号目录,不写 PlayerAllData

`go/login/internal/logic/clientplayerlogin/createplayerlogic.go`:

| 步 | 做什么 | 出处 |
|---|---|---|
| 3 | 拿账号建角锁 `account_lock:create:{account}`,TTL 20s | :116-130;`login.yaml:83` |
| 4 | 读账号 blob | :132-154 |
| 6b | 号段铸 `player_id`,失败就整体拒绝 | :219-229 |
| 6c | 名字登记到 data_service 全局库,fail-closed | :231-255 |
| 6d | 登记 `player:zone`,fail-closed;`PinOnCreate` 时同一次 RPC 原子钉落点 | :257-274、:707-717 |
| 7 | 以建角锁为围栏写账号 blob;结果不确定时回读 | :287-301、:54-73、:621-656 |
| 7b | 写反查索引 `player_id → account` | :303-308 |

- 没有任何一步写 `PlayerAllData:{id}`。
- `player_class_backfill.go:18-20` 的注释原话就是:「PlayerAllData 在建角时还不存在,首次 EnterGame 才由预加载建出」。

### 2.2 首次 EnterGame:异步链里的预加载

`entergamelogic.go`:

- **入口**:拿玩家锁(:97-114),校验归属(账号 blob,或反查索引 + self-heal,:139-219)。
- **RPC 立即回「已受理」**,真正进场走异步链(:221-244)。
- **预加载**:放进 PreloadPool 执行 `EnsurePlayerAllDataInRedisAsync`(:381-391)。
  - 池容量 256,池满直接回 `kLoginInProgress`(:393-400;`login.yaml:332-334`)。
  - 每个读任务等结果的上限是 `dispatcherTaskTTL = 8s`(:356-380)。
  - **回调在哪个 goroutine 上跑**:
    - 快路径命中时,`onComplete` 在池 worker 里同步执行(`ensure_player_all_data_async.go:65-67`)。于是整段 `applyLoadedPlayerSession`(GetSession、身份补齐、SetSession、BindSession、EnterScene,:333、:685 起)都占着这个 worker。
    - 冷读路径的池 worker 发完 Kafka 就返回;apply 跑在 dispatcher 的 16 个 worker 或溢出 goroutine 上(`dispatcher/task_result_dispatcher.go:82`、:288-300)。

`go/login/internal/logic/pkg/dataloader/ensure_player_all_data_async.go`:

1. 先构造骨架 `{player_database{player_id}, player_database_1{player_id}}`(:49-52)。
2. 父键 `EXISTS` 命中,直接 `onComplete(nil)`。这就是快路径(:55-68)。
3. 父键不在,逐张子表 `GET` 子表缓存;全部命中就组装父键并写入(:80-109)。
4. 缺哪张子表,就为它发一条 Kafka 读任务(:157-207;`sync_loader.go:81-110`,`Op=read`,`WhereCase player_id='…'`)。
   - 回执经 `task:result` 的 Pub/Sub 送回。频道全局只有一个(`dispatcher/task_result_dispatcher.go:32`)。
5. 回执 `Data` 为空表示没有这一行,子表保持骨架(:180-190)。
6. 全部回齐后 `SET` 父键,TTL 24h(:128-155;`sync_loader.go:121-125`)。

对新号来说:父键不在(建角没写),子表缓存也不在,所以**必然**发 2 条读任务,并同步等它们回来。

### 2.3 go/db 的读任务

- 按 `MsgType` 把请求体反序列化成 msg(`go/db/internal/kafka/key_ordered_consumer.go:1994-2001`,工作区行号)。
- `handleDBReadOp` 在选中的落点库上执行 `FindOneByWhereClause`。查不到行(`ErrNoRowsFound`)不算错,msg 保持请求体原样(:339-348)。
- **选库顺序**:落点记录 → home_zone → 本进程 zone(`placement_route.go:178-189`)。
  - `Required=true` 且没有落点记录时,读回执直接失败(:297-305)。
- **结果回写**:
  - 写子表缓存 `{MsgType}:{id}`,TTL 取 `DefaultTTLSeconds`,K8s 上是 3600s(`key_ordered_consumer.go:54-64`、:360-373;`k8s_deploy.ps1:3458`)。
  - 再 `LPUSH task:result:{taskID}` + `EXPIRE` + `PUBLISH task:result:notify`(:391-414)。
- **新号冷读的最终产物就是「请求体原样」,也就是骨架**。
- **资源共享**:
  - 读任务和存盘写任务共用本 zone 10 分区 × 8 子分片的 worker(`go/db/etc/db.yaml:19`、:22),以及每实例 60 条的主池(:68)。
  - Phase 2 打开 `PinOnCreate` 后,读的是全局落点库,走每实例 8 条的额外池(`db.yaml:122-124`。注释自己写了这个池「会成为全局库的瓶颈」)。

### 2.4 身份补齐

`player_class_backfill.go`:

- `applyLoadedPlayerSession` 先 `GetSession`,再调 `backfillPlayerIdentity`(`entergamelogic.go:685-695`)。
- 先读父键(:35-47)。读不到就报错,这条链失败,客户端重试。
- 职业、名字、外观、性别,缺哪个补哪个(:57-83)。
- 用 Lua 做整字节 CAS 写回(:88-106、:179-191):
  - 校验玩家锁令牌;
  - `player:session:{id}` 或 `player:{id}:location` 存在就跳过;
  - `SET … KEEPTTL`,保留原有 TTL。

### 2.5 scene 加载与新号初始化(C++)

1. **铸 epoch**:scene_manager 的 EnterScene 用 INCR 铸 `owner_epoch`(`go/scene_manager/internal/logic/owner_epoch.go:55-119`),再把玩家路由到 scene。
2. **登记与加载**:scene 的 `PlayerEnterGameNode` 先登记待入场,再清继承来的交接标记,最后对父键发 `AsyncLoad`(`cpp/nodes/scene/handler/rpc/scene_handler.cpp:229-277`)。
3. **NIL 重试**:读到 NIL 时按 500ms / 1s / 2s / 4s / 8s / 16s 有界重试,合计约 31.5s(`redis_client.h:389-406`)。
   - 同文件 :103-105 的注释写的是「2,4,…60s,约 2 分钟」,已经过时。
   - 重试用完仍是 NIL,就按 `DataNotFound` 喂一份空 `PlayerAllData`,当新号处理(`player_lifecycle.cpp:1911-1930`)。
   - 零长度值和解析失败一律按 `RedisError` 处理,fail-closed(`redis_client.h:846-862`)。
4. **`HandlePlayerAsyncLoaded`**:`player_id==0` 时先补上 id(`player_lifecycle.cpp:2037-2052`),再进 `InitPlayerFromAllData`(:3495-3556)。
5. **首次注册**:`registration_timestamp <= 0` 就算首次注册。写注册时间、等级置 1、触发 `RegisterPlayerEvent`(:3529-3537),然后进场景并发出 `SNAPSHOT_LOGIN` 快照(:3555)。
6. **结论**:scene 判断「新号」看的是 `registration_timestamp`,不是父键在不在。冷读骨架、`DataNotFound` 兜底、种子,这三种输入走的是同一个分支。

### 2.6 首次存盘

- **写 Redis**:`SavePlayerToRedisImpl`(`player_lifecycle.cpp:3565` 起)。
  - `owner_epoch≠0` 时走带守卫的 Lua。KEYS 是父键和 `player:{id}:owner_epoch`(`redis_client.h:42-52`)。
  - 否则走 `SET` + `SADD dirty_keys_set`(:20-24)。
  - 两条路都是不带 `KEEPTTL` 的 `SET`,父键从此不再过期。
- **写库**:两张子表各发一条 DBTask(`op=write`,带 `owner_epoch`,topic 按 home_zone 选)(`player_lifecycle.cpp:3693-3737`)。go/db 按落点路由、比对 epoch 后落库。
- **含义**:新号**第一次碰数据库**本来就可以是这条异步写。冷读给不了写路径任何它需要的东西。

### 2.7 每个新号首次进场的代价(推算,未计数)

- **login**:2 条 Kafka 读任务、dispatcher 两次登记、等回执(上限 8s)、一次父键 `SET`。
- **go/db**,每条读任务:
  - 选库和复核各一次落点 `MGET`(`placement_route.go:19-30`);
  - 一次点查;
  - 一次子表缓存回写;
  - 一组 `LPUSH` / `EXPIRE` / `PUBLISH`。`PUBLISH` 走单频道,会扇出到所有 login 实例。
- **两点后果**:
  - 这些都发生在玩家的加载屏上,是同步等待。
  - 它们和存盘写共用同一组 worker 和连接池。

## 3. 方案比选

| 方案 | 做法 | 改动面 | 出错时的形态 | 结论 |
|---|---|---|---|---|
| **A 建角写种子(只带 player_id)** | 写账号 blob 前用 Lua 写 `PlayerAllData:{id}` = 冷读骨架,`PX` 24h;只在父键和 owner_epoch 键都不存在时写 | login 3 个文件 + 配置;C++、go/db 零改动 | 种子缺席就走冷读老路,结果相同 | **选定** |
| A1 种子带身份 | 种子里直接写职业 / 名字 / 外观 / 性别 | 同 A | 同 A | 不选。身份写法会有建角、backfill 两份(违反 DRY);能省的只是首次进场一次 Redis CAS |
| A2 并进账号 blob 的围栏脚本 | 在 `writeAccountBlobScript` 里原子地顺手写种子 | 改围栏脚本,要两个版本 | 同 A | 不选。把缓存优化和账号目录这份权威写绑死;L-06 拆 Redis 后两把键不再同实例,原子性保不住 |
| B1 新号标记,由 scene 按新号初始化 | 建角写 `player:{id}:new`(短 TTL);预加载看到标记就放行,不写父键 | login,加上 scene_manager 路由事件加字段,加上 C++ | scene 读父键得 NIL,要等约 31.5s 有界重试才判新号(`redis_client.h:389-406`)。想跳过,就得给 `RoutePlayerEvent` / `PlayerEnterGameNode` 加「新号」位,proto、Go、C++ 三处同改。标记过期前若已有存盘、父键又丢了(合服失效缓存、Redis 丢键),scene 会用空档覆盖真实进度 | 否决 |
| B2 新号标记,login 看到标记就写空父键 | 首次进场时,login 原子地执行「标记在、父键不在 → 写骨架、删标记」 | login | 要安全,必须再加「owner_epoch 不存在」这个判据;加上之后标记本身就多余了 | 否决。等于把 A 推迟到首次进场,还多一个键、多一个状态源 |
| C1 账号记录加 `first_enter_pending` | 在账号 blob 里标「未进场」 | login | 和 B 一样有「标记已过时却仍被当真」的问题;清标记要再拿一次账号锁写 blob | 否决 |
| C2 建角后异步预热 | 建角后 fire-and-forget 发读任务(`TriggerPlayerDataPreload`,`sync_loader.go:35-78`,目前没有调用方) | login | 等待挪出了玩家路径,但每个新号的 2 次 DB 读一次不少,全局库首日的读负载不变;子表缓存 1h 过期后,首登仍要冷读 | 否决,解决不了负载 |
| C3 go/db 负缓存 | 把「无行」结果缓存起来 | go/db | 第一次读仍然要打库 | 否决 |
| C4 scene 读到 NIL 立刻当新号 | 去掉 NIL 重试 | C++ | scene 分不清「新号」和「缓存丢了的老号」,老号会被空档覆盖 | 否决 |

## 4. 选定方案

### 4.1 种子内容

- 种子 = `newPlayerAllDataSkeleton(id)` = `{player_database{player_id}, player_database_1{player_id}}`。
- **不带身份,也不带 `registration_timestamp`**。带了注册时间,scene 就会跳过首次注册(`player_lifecycle.cpp:3529`)。
- 冷读骨架(`ensure_player_all_data_async.go:49-52`)也改成调用这同一个函数,两边始终同源。

### 4.2 写入点和顺序

写在 CreatePlayer 的 6d(归属登记)成功之后、6e/7(账号 blob)之前。

- **为什么在 6d 之后**:6c、6d 都是 fail-closed。建角失败时,种子根本不会写。
- **为什么在 7 之前**:
  - EnterGame 的归属校验只认账号 blob 或反查索引(`entergamelogic.go:155-219`),这两样在 7 / 7b 才写。
  - 所以种子落地之前,任何 EnterGame 都用不上这个 id。首次进场看到的父键只有两种可能:种子,或者缺席。
- **崩溃恢复**:AOF 和复制都是按序的,恢复后的数据是一个前缀。账号 blob 还在,种子就一定也在(除非已过期)。
- **代价**:多一次 Redis 往返,落在账号锁内(§9 第 1 条)。

### 4.3 写入脚本的语义

脚本是 Lua。`KEYS` = 父键、`player:{id}:owner_epoch`;`ARGV` = 种子字节、TTL 毫秒。按顺序判断:

1. owner_epoch 键已存在:返回 -1,不写。
2. 父键不存在:`SET … PX`,返回 1。
3. 父键的值正好就是本次种子:返回 2,算成功。这是 go-redis 读超时后,把已经执行过的 EVAL 又重发了一次。
4. 其余情况:返回 0,不写。

**为什么要看 owner_epoch**:

- scene_manager 每次把玩家派进场景都会 INCR 这个键,而且从不删除(`go/shared/ownerepoch/ownerepoch.go:16-22`;`redis_client.h:31-33`)。
- 键存在,说明玩家进过场景。这时 scene 的存档才是真相,种子必须让路。
- 它兜住的是一种极端交错:种子请求在网络里滞留,等它落地时,玩家已经进场、存过盘,父键又恰好被删了(§6 第 5 行)。

### 4.4 读侧完全不改

以下都不动:

- `EnsurePlayerAllDataInRedisAsync` 的 `EXISTS` 快路径;
- 身份补齐;
- scene 加载;
- 存盘;
- go/db。

### 4.5 开关、指标、日志

| 项 | 内容 |
|---|---|
| 开关 | `login.yaml` 的 `NewPlayerSeed.Enabled`,bool,缺省 false;整块不写也是关 |
| 指标 | `login_create_player_seed_total{result}`,result ∈ written / idempotent / parent_exists / owner_epoch_exists / error |
| 指标 | `login_create_player_stage_seconds{stage="seed"}` |
| 启动日志 | `[new-player-seed] enabled=… (pin_on_create=…)` |
| 看效果 | `dataloader_preload_cache_check_seconds_count{hit="miss"}` 下降;go/db `db_task_result_total{op="read"}` 下降 |

## 5. 不变量

| # | 不变量 | 依据 |
|---|---|---|
| I1 | **新铸 id 从未被使用过** | 号段由 data_service 用 CAS 领段;客户端拒绝重叠或回退的范围(`go/shared/idsegment/client.go:26-34`)。默认不回退到 snowflake(`login.yaml:184-189`;`player_id_minter.go:22-29`);即使回退,snowflake 的值域 ≥2^55,和号段不相交(`client.go:36-51`)。全系统本来就依赖这一条:`player_database` 撞号不报错,而是静默串档(`createplayerlogic.go:219-222`) |
| I2 | **种子 ≡ 冷读读空后的组装结果** | 两边用同一个构造函数;§8 有等价性用例 |
| I3 | **种子永不覆盖已有数据** | 脚本只在父键和 owner_epoch 键都不存在时写 |
| I4 | **种子先于 id 对外可见** | §4.2 |
| I5 | **种子 TTL 24h,和冷读组装的父键相同;scene 首存后父键不再过期** | `sync_loader.go:121-125`;`redis_client.h:20-24`、:42-52 |
| I6 | **父键和 owner_epoch 在同一个 Redis 实例** | 这是既有契约:C++ 守卫 Lua 在同一个脚本里访问这两把键(`redis_client.h:42-52`);CZ-2 写明「PlayerAllData Redis 物理共享」(`cross-zone-scene-travel.md:31`)。种子没有引入新的同址约束 |
| I7 | **种子不是真源** | 任何失败都不改变建角结果,首次进场退回冷读 |

## 6. 失败与并发分析

| # | 场景 | 会发生什么 | 结论 |
|---|---|---|---|
| 1 | 号段 id 不复用(万一复用了) | 父键或 owner_epoch 已存在:不写,计 `parent_exists` / `owner_epoch_exists`,打 ERROR;首次进场读到的是已有缓存,和今天一样。都不存在、但库里有旧行(要求旧玩家的父键和 owner_epoch 在 Redis 里也都没了):种子让新角色从空档起步,首存**覆盖**旧行;今天则是冷读把旧行读给新角色,两个账号共用一份档。两者都是 ID 安全事故,但形态不同:今天是旧角色的资产串给新账号,旧行还在;开种子后资产不外泄,但旧玩家在库里的进度被覆盖,只能靠快照或备份找回 | 两种都不可接受,只能靠 I1 兜底。种子在 Redis 侧多给一个检测信号。是否升级为拒绝建角见 §10 P2 |
| 2 | 建角在种子之前失败(发号 / 名字 / 归属) | 种子没写 | 无残留 |
| 3 | 建角在种子之后失败:账号 blob 围栏写回 -1 或报错,回读确认不含新角色(`createplayerlogic.go:621-656`) | 留下一个只带 player_id、24h 后过期的孤儿种子。这个 id 已经烧掉,没人能用它进场。约百字节 | 无害 |
| 3' | 回读本身失败(`accountBlobUnknown`) | 角色可能已经落盘,而种子恰好就是它正确的初始值 | 无害 |
| 4 | login 在种子前后崩溃 | 和第 2、3 行相同。崩在 EVAL 执行中途:要么写了,要么没写 | 都安全 |
| 5 | 种子写入结果未知,或请求滞留之后才落地 | 落地那一刻:父键 = 骨架(刚被冷读写入)→ 返回 2,不写;父键已被身份补齐改过 → 返回 0;玩家已经进场 → owner_epoch 存在,返回 -1 | 滞留的种子什么时候落地,都覆盖不了进度 |
| 6 | 种子和首次存盘的竞态 | 首存是无条件 `SET`,会覆盖种子,这是应该的;种子是条件写,永远覆盖不了首存 | 无论谁先谁后,都收敛到 scene 的数据 |
| 7 | 种子和冷读并发 | 只会在种子写失败或结果未知时出现。冷读写的也是骨架 | 结果一样 |
| 8 | 跨 zone 首登 | 父键在共享 Redis 里。K8s 上相关组件全部同址:C++ zoneredis(`k8s_deploy.ps1:1866-1868`)、login RedisClient(:3583-3586)、go/db(:3456)、scene-manager(:3720 起)。所以任何 zone 的 login 都能命中。归属重定向和访客照今天的规则走(`entergamelogic.go:646-683`);存盘按 home_zone 选 topic(CZ-2)。zone_redis 物理分开的部署本来就不支持(`cross-zone-scene-travel.md:31`) | 不变 |
| 9 | Phase 2 `PinOnCreate` | 种子和落点无关。打开 PinOnCreate 后,新号第一次碰全局落点库的是异步首存,不再有同步读 | 去掉了全局库首日唯一的同步读;误配置后的症状变化见 §9 第 2 条 |
| 10 | owner_epoch | 种子不铸 epoch,也不读它的值,只看键在不在。scene 首存的守卫补种和 CAS 照旧(`redis_client.h:31-40`)。旧版 scene_manager 不铸 epoch 的兼容窗口里,第 5 行的「玩家已进场」判据退化为只看父键 | 兼容窗口内只剩第 5 行的极端交错,且需要父键被删才会触发 |
| 11 | C++ 对空数据的处理 | 种子非空、`player_id≠0`、`registration_timestamp=0`,进首次注册分支,和冷读骨架同一路径(§2.5)。零长度和解析失败照旧 fail-closed | 不变 |
| 12 | Redis 丢键(重启丢最后 1s 的 AOF、人工删、合服失效) | 首次进场冷读,读到空 | 结果相同 |
| 13 | 合服 | copy 模式按清单删玩家缓存键,含 PlayerAllData(`tools/merge_zone/player_rows.go:676-723`;`unmerge.go:251-263`)。种子被删后在 dst 冷读,读到空。pin 模式保留缓存,种子仍是真值。合服在停服期间进行,窗口内没有在途建角。owner_epoch 不会被删,滞留的种子仍被挡住 | 安全 |
| 14 | zone 回档 | 灾难回档要 FLUSHDB,带共享键防护(`zone_data_rollback.md:163-170`),种子随之消失,首次进场冷读。应用级回档不碰没有快照的玩家:快照要进场后才有(`player_lifecycle.cpp:3555`)。孤儿只报告、不删除(`go/data_service/internal/logic/rollback_logic.go:1050-1090`) | 安全 |
| 15 | TTL 边界 | 种子恰好在「预加载 EXISTS 命中」和「scene GET」之间到期。① 到期发生在身份补齐之前:补齐读不到父键,链失败,客户端重试,改走冷读。② 发生在补齐之后、scene GET 之前:scene 先 NIL 重试约 31.5s,再按 DataNotFound 初始化空档;这一次会话里职业和名字为空,下一次干净登录时由身份补齐修正。窗口是一次进场链的耗时(百毫秒级)比上 24h。今天的首次进场碰不到这个窗口,因为冷读刚写的父键带满 24h;只有「预加载过、但还没被 scene 存过盘」的父键在下一次进场时会碰到。种子把它扩到「建角后约 24h 才首次进场」的新号 | 残余风险;可选加固见 §10 P4 |
| 16 | `player-storage-placement.md` §13 第 3 条的违规配置:`Required` 开着而新号没落点;`NewPlayerStorageId` 指向打不开的库 | 见 §9 第 2 条 | 要求两类告警先就位 |
| 17 | go/db 或 Kafka 不可用 | 新号仍能进场,存盘在 Kafka 里排队,和老玩家一样。今天新号会卡 8s 后失败 | 可用性变好 |
| 18 | 滚动升级 / 新旧混跑 | 新旧 login 的读路径相同,旧 login 只是不写种子。开关随时可关;已写的种子 ≡ 冷读结果,24h 自然过期 | 无需清理 |
| 19 | 展示类读者 | 种子不含身份,首次进场照旧由身份补齐写入。好友资料、外观回填读到种子时,按「缺字段」处理(`go/friend/internal/data/friend_profiles.go:44-62`;`role_appearance_backfill.go:50-52`),展示结果和读不到 blob 时一样 | 不变 |
| 20 | 资产账本 | 对未进场的新号,data_service `GetPlayerAssetOpLedger` 的结果从 absent 变成 found + 空账本(`asset_op_ledger_logic.go:36-44`、:117-123)。guild / trade 两种情况都判 UNKNOWN,继续等(`go/shared/assetop/classify.go:144-162`) | 语义不变,只是计数 label 变了 |
| 21 | 入场链占用哪个池(§2.2) | 开种子后,新号也走快路径:`applyLoadedPlayerSession` 整段在 PreloadPool 的 worker 里同步跑完,不再在 dispatcher worker 上跑。每个新号占用池 worker 的时间,从「发完 Kafka」变成「整段 apply」,其中含 player_locator、scene_manager 两次 gRPC 和一次 Kafka 同步发送。池是非阻塞提交,Size 256(`login.yaml:332-334`),池满回 `kLoginInProgress`,客户端重试。另一面,dispatcher 不再替新号跑 apply,`cb_wait` 的排队压力变小。老玩家命中缓存今天就是这条路 | 背压形态变了,不是正确性问题。L-14 开、关两轮都要看 `[preload-pool] dropped_delta` 日志和 `entergame_apply_*` 各阶段;出现掉单就调大 `PreloadPool.Size`(只改配置) |

## 7. 上线与回退

**上线步骤**

1. 落码,默认关。Codex 跑单测(§8)和 Part B 列的验证命令。
2. 本地冒烟:临时打开 `login.yaml` 的开关,用 robot 建 N 个新号进场,核对指标。
3. 告警先就位:
   - go/db `db_placement_guard_total{op="write",outcome="missing_required"}`(违规配置一:Required 开着而新号没落点);
   - go/db `db_placement_store_open_total{result=~"error|rejected"}`,以及 go/db 的 ERROR 日志 `retry task rescheduled`,其 cause 含 `placement store is unavailable`(违规配置二:`NewPlayerStorageId` 指向打不开的库。写任务按 `errPlacementStoreUnavailable` 延后、不耗重试预算,会一直在重试队列里转,`placement_route.go:36-37`、`key_ordered_consumer.go:2072-2107`);
   - `login_create_player_seed_total{result=~"parent_exists|owner_epoch_exists"}`。
4. 开服尖峰压测(L-14)里开、关各跑一轮,按 AGENTS §6 出对比表:首次进场 p99、db_task 读量、Redis ops,以及 PreloadPool 掉单(login 日志 `[preload-pool] dropped_delta`)、`kLoginInProgress` 次数、`entergame_apply_*` 各阶段(§6 第 21 行)。
5. K8s 打开:login ConfigMap 生成器加上这一键。单独一批做,契约测试同步改。

**回退**

- 改回 false 并滚动重启 login(go-zero 配置不热加载),或回滚 login 镜像即可。
- 已写的种子和冷读结果相同,24h 后过期;不需要任何数据迁移。
- 打开和关闭都不要求和 C++ / go/db / scene_manager 同批发布。

## 8. 测试

| 层 | 用例(名字见 Part B §B5) | 外部依赖 |
|---|---|---|
| dataloader | 种子内容与 TTL;重发幂等;不覆盖已有父键;owner_epoch 存在就拒写;零 id / nil 客户端;Redis 不可用;**种子 ≡ 冷读骨架**(子表缓存给出「无行」时,`EnsurePlayerAllDataInRedisAsync` 写出的父键和种子 `proto.Equal`);种子命中后快路径不碰 Kafka 和 dispatcher | 无(miniredis) |
| CreatePlayer | 开关关 → 不写;开关开 → 写骨架并带 TTL;归属登记失败 → 不写;账号 blob 围栏失败 → 只留会过期的种子;已有父键 → 不覆盖并计数;owner_epoch 已存在 → 不写并计数;种子脚本报错 → 建角照常成功并计 `error` | 无(miniredis + 假 data_service) |
| EnterGame | 种子 → EnterGame:走快路径,身份补齐写进种子,TTL 保留,没有子表缓存键 | 无(miniredis + ants 池 + 假 player_locator) |
| 配置 | 整块缺失 = 关;两份样例 yaml 显式写出且为关;探针拒绝拼写错误 | 无 |
| C++ | 不改代码,不新增用例。scene 的新号初始化分支今天也被冷读路径走着,没有单测是既有缺口(§10 R7) | — |
| 端到端 | 本地全栈冒烟(可选),开服尖峰压测(L-14) | 本地 MySQL / Redis / Kafka / etcd / C++ 节点 |

## 9. 与现有设计的偏离与代价

1. **建角持锁时间多一次 Redis 往返**。
   - ctx 预算 1s,只能掐掉 go-redis 的重发;单次最坏仍是 3s 读超时。
   - `login.yaml:65-82` 的锁 TTL 预算注释要同步更新,而且要写出真实的数:
     - 原注释的最坏和是 gRPC 3×3s + Redis 3×3s(读 blob、围栏写、回读)= 18s,「20 盖住」成立;
     - 开种子后 Redis 变成 4 次,最坏和是 21s,**超过** `AccountLockTTL: 20`。
   - 原注释的口径本来就是「20 不是硬上界」,正确性靠围栏 + 回读(:78-81)。本批不改 TTL,只把注释改成如实的数;是否上调见 §10 P6。
   - zrpc 服务端 `Timeout: 10000`(`login.yaml:7-13`)的注释按「常态 Redis 毫秒级」估算。种子在常态下也是毫秒级,所以不用改。Redis 变慢时今天的路径本来就可能超 10s,种子会再多占最多 3s。
2. **失去一个早期信号:`player-storage-placement.md` §13 第 3 条的两种违规配置**。
   - `login.yaml:229-231`、`deploy/login-stack.linux/login.yaml:150`、`config.go:163-166` 和 `player-storage-placement.md:448` 写的症状都是「新号进游戏预加载秒级失败、存盘全进死信」。
   - 开种子后,新号不再走预加载读,症状只剩存盘这一半:
     - Required 开着而新号没落点:存盘全进死信(`placement_route.go:297-303`)。
     - `NewPlayerStorageId` 指向打不开的库:写任务按 `errPlacementStoreUnavailable` 延后,不耗重试预算,一直在重试队列里转(`placement_route.go:36-37`、`key_ordered_consumer.go:2072-2107`)。今天读任务会耗尽重试,新号进不去,所以很响亮。
   - 两种情况下玩家都能进场。进度只在 Redis 父键里(首存后不过期),不进 MySQL;Redis 丢键或合服 copy 模式失效缓存时,进度就真的丢了。
   - 所以要求:打开种子之前,§7 第 3 步的 `missing_required` 和 `db_placement_store_open_total{result=~"error|rejected"}` 两类告警必须到位;§13 第 2、3 条的打开顺序不变。上面四处症状描述都补一句「开着 NewPlayerSeed 时只剩存盘这一半」(B2.7、B2.9、B2.10、B2.11)。
3. **Redis 内存**。未进场角色的种子和孤儿种子各约百字节,24h 后过期。进过场的玩家,父键今天就常驻。
4. **压测口径**。开关前后,dataloader 各阶段表和 entergame preload 分布都会变。压测对比时必须注明开关状态。
5. **注释过时**。
   - `player_class_backfill.go:18-20`:本批一起改。
   - `player_lifecycle.cpp:1916-1922`「CreatePlayer only writes account meta」、`player_lifecycle.cpp:2037-2041`「CreatePlayer only creates an account entry」:C++ 注释,随下一个 C++ 批次改,不为一句注释单独跑 MSBuild。
6. **和 L-05 原文一致**。L-05 写的是「建角时写一个空的 PlayerAllData」,本方案就是这个,不是「新号标记」。
7. **新号入场链的背压形态变了**(§6 第 21 行)。
   - 今天新号的 apply 跑在 dispatcher worker 上,开种子后跑在 PreloadPool worker 上。
   - 后者容量有上限,而且是非阻塞提交,池满回 `kLoginInProgress`。
   - 这条路老玩家命中缓存时今天就在走,不是新机制;但开服首日新号占大头,池的占用时长会明显变长。
   - L-14 压测必须观察池掉单;出现掉单就调大 `PreloadPool.Size`。

## 10. 风险与待负责人拍板

### 风险

| # | 风险 | 处置 |
|---|---|---|
| R1 | 整个方案建立在「号段 id 不复用」上,这也是全系统的前提。万一复用、而 Redis 侧旧状态也没了,串档形态会从「旧行被新账号继承」变成「旧行被新角色首存覆盖」(§6 第 1 行) | 种子顺带提供了检测信号:`parent_exists` / `owner_epoch_exists` 计数 + ERROR 日志 |
| R2 | TTL 边界(§6 第 15 行) | 默认接受;可选加固见 P4 |
| R3 | §13 第 3 条的两种违规配置变隐蔽(§9 第 2 条) | 两类告警作为打开前置 |
| R4 | L-06 拆 Redis 时,父键、owner_epoch、身份补齐 Lua 里的会话 / 位置 / 玩家锁键必须仍在同一实例 | 这是既有约束,种子没有新增。拆分方案里要点名这一组 |
| R5 | 账号锁内多一次 Redis;Redis 卡顿时最坏多 3s | 正确性靠围栏,不受影响;观察 `stage="seed"` |
| R6 | 收益没有实测 | 走 L-14 对比表;拿不出表不下性能结论 |
| R7 | C++ 新号初始化分支没有单测 | 既有缺口,种子没新增风险;可在后续 C++ 批次补 |
| R8 | 新号入场链改占 PreloadPool,池满回 `kLoginInProgress`(§6 第 21 行、§9 第 7 条) | L-14 看池掉单;按需调大 `PreloadPool.Size` |

### 待拍板

| # | 事项 | 本稿默认 |
|---|---|---|
| P1 | 采用方案 A(种子只带 player_id,不带身份) | 采用 |
| P2 | id 复用信号是否升级为拒绝建角 | **只告警**。走到种子这一步时 `player:zone` 已登记;此时拒绝,会留下永久的幽灵映射(`createplayerlogic.go:257-263`) |
| P3 | 何时在 K8s / 开服档打开 | L-14 对比表出来、告警就位之后;另起一批改 `k8s_deploy.ps1` |
| P4 | TTL 边界加固:快路径命中、且父键剩余 TTL 低于阈值时续期 | **不做**。窗口极小,且能自愈 |
| P5 | `missing_required` 与 `db_placement_store_open_total{result=~"error|rejected"}` 两类告警是否作为硬门禁 | 是 |
| P6 | 开种子后 `AccountLockTTL` 是否从 20 上调到 ≥22,让「gRPC 拖满 + Redis 慢但回包」的最坏和(21s)仍被盖住(§9 第 1 条) | **不调**。20 本来就不是硬上界,正确性靠围栏;上调的代价是 login 崩溃后同账号建角被挡得更久。`k8s_deploy.ps1` 从 `login.yaml` 读这个值,要调只改 `login.yaml` 一处,K8s 自动跟随 |

## 11. Java 版(AGENTS §12)

**状态:不适用**(服务端内部实现不同,客户端契约不变)。

- Java 建角在事务里直接插入玩家行(`game-login/.../CreatePlayerHandler.java:222` `createPlayerWithinCap`)。
- 进场时按主键读本库:`EnterGameHandler.java:273` `findPlayer`;scene 侧 `StoragePlayerRepository.java:183-195`。
- 所以 Java 没有「读一行不存在的数据、还要经队列同步等待」这条链,也就没有要优化的冷读。
- 本功能不改任何客户端可见的消息、tip 码或配表。按 §12,在 `PARITY.md` 登记一行「不适用」即可。

## 12. 和「全服库成了瓶颈怎么办」的关系

- **「全局库」有两个,要分开看**:
  - data_service 全局库:放名字、号段、流水、快照,以及 D1 的 `player_route`;
  - Phase 2 的玩家主数据全局落点库 `player_store_1000000_db`。
- **G3 处理的是后者首日唯一的同步读路径**(tidb-phase1 L-05;`db.yaml:122-124`)。打开种子后:
  - 新号首日对全局落点库只剩异步写。它有 Kafka 削峰,积压表现为 lag,不会让玩家卡在加载屏。
  - 老玩家的父键常驻 Redis(scene 存盘会去掉 TTL),也不冷读。
  - 于是全局落点库从「玩家同步等待的依赖」变成了「异步落库的下游」。
- **写侧瓶颈仍在,不归 G3 解决**:单调主键的尾部 Region、每实例 8 条的额外池、每 zone 固定 10 分区。见 tidb-phase1 §5、L-08、L-13。
- **归属目录(D1 `player_route`)**每个玩家一生只写一次,见提案 §12。
<!-- END docs/design/new-player-first-enter-no-cold-read.md -->

---

# Part B —— 实现规格

## B1 文件清单

### 新增(6 个)

| 文件 | 要点 |
|---|---|
| `go/login/internal/logic/pkg/dataloader/new_player_seed.go` | `newPlayerAllDataSkeleton`(冷读与种子共用)、`SeedResult`、`seedNewPlayerScript`、`SeedNewPlayerAllData` |
| `go/login/internal/logic/pkg/dataloader/new_player_seed_test.go` | 8 个 miniredis 用例,含「种子 ≡ 冷读骨架」等价性用例 |
| `go/login/internal/logic/clientplayerlogin/createplayer_seed.go` | `newPlayerSeedTimeout`、`(*CreatePlayerLogic).seedNewPlayer`:尽力而为,只计数和打日志 |
| `go/login/internal/logic/clientplayerlogin/createplayer_seed_test.go` | 7 个 CreatePlayer 用例 + 1 个 EnterGame 快路径用例 |
| `go/login/internal/config/new_player_seed_conf_test.go` | 4 个配置用例(仿 `placement_conf_test.go`) |
| `docs/design/new-player-first-enter-no-cold-read.md` | Part A 全文 |

### 修改(11 个)

| 文件 | 要点 |
|---|---|
| `go/login/internal/logic/pkg/dataloader/ensure_player_all_data_async.go` | :49-52 的骨架改为调用 `newPlayerAllDataSkeleton`;**删掉 :8 的 `login_proto_database` import**,它只在这里用,不删会编译失败 |
| `go/login/internal/logic/clientplayerlogin/createplayerlogic.go` | 在 :274 和 :276 之间插入 6d' 步 |
| `go/login/internal/logic/clientplayerlogin/metrics.go` | 新增 `createStageSeed`、`createPlayerSeedTotal`;改 Help 文案和 stage 注释;`PrimeCreatePlayerMetrics` 预置两个异常结局并改函数注释;加 dataloader import |
| `go/login/internal/logic/clientplayerlogin/player_class_backfill.go` | 只改 :18-20 的注释 |
| `go/login/internal/config/config.go` | :103 后加 `NewPlayerSeed` 字段;:188 后加 `NewPlayerSeedConf` 类型;:166 后在 `PlacementConf` 注释补一句 |
| `go/login/internal/svc/home_zone.go` | :40 后加一行启动日志 |
| `go/login/etc/login.yaml` | :235 后加新块(显式关);:68-69 之后改写锁预算注释(如实写 21s);:231 后在 Placement 第 3 条补一句 |
| `deploy/login-stack.linux/login.yaml` | :150 后在 Placement 第 3 条补一句;:154 后加显式关闭的块。拼写探针测试会读这个文件 |
| `docs/README.md` | 「玩家数据与存储」分类里插一行(B7) |
| `tools/scripts/gen_docs_index.py` | 「玩家数据与存储」规则加 `"new-player-*"`(B7) |
| `docs/design/player-storage-placement.md` | :448 段末补一句(推荐做;他人有未提交改动,只追加、不改原句) |

### 不改

C++ 全部、go/db、scene_manager、data_service、proto、`tools/scripts/k8s_deploy.ps1`、AGENTS.md、docs/PROGRESS.md、Java 仓库。

### 后续批(不在本批)

1. `k8s_deploy.ps1` 的 login ConfigMap 加 `NewPlayerSeed.Enabled`,同步改契约测试。待 P3 拍板。
2. C++ 三处过时注释:`player_lifecycle.cpp:1916-1922`、`player_lifecycle.cpp:2037-2041`、`redis_client.h:103-105`。
3. `stress_summarize.ps1` 的 dataloader 表加 hit/miss 计数列(可选)。
4. Docs 阶段:`docs/PROGRESS.md` 追加一条;`tidb-phase1-and-launch-readiness.md` 的 L-05 标状态;Java `PARITY.md` 登记「不适用」一行。

## B2 逐文件代码

### B2.1 新增 `go/login/internal/logic/pkg/dataloader/new_player_seed.go`

```go
package dataloader

import (
	"context"
	"fmt"
	login_proto_database "proto/common/database"
	"shared/ownerepoch"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// newPlayerAllDataSkeleton 返回「这名玩家在库里一行都没有」时父键的完整内容:两张子表各只带 player_id。
//
// 两条路共用这一个构造,必须同源(docs/design/new-player-first-enter-no-cold-read.md §4.1):
//   - EnsurePlayerAllDataInRedisAsync 冷读时以它为底。go/db 查不到行时原样回显请求体
//     (go/db key_ordered_consumer.go 的 handleDBReadOp / publishDBReadResult),子表保持这份内容;
//   - SeedNewPlayerAllData 在建角时直接写它。
//
// 所以新号的种子与「冷读读空后组装出的父键」逐字段相同,首次入场之后的身份补齐(player_class_backfill.go)
// 与 scene 的新号初始化(InitPlayerFromAllData 的 registration_timestamp 分支)走的是同一条既有路径。
// 这里**不能**带身份字段或 registration_timestamp:前者有 backfill 负责,后者一旦非 0,scene 会跳过首次注册。
func newPlayerAllDataSkeleton(playerId uint64) *login_proto_database.PlayerAllData {
	return &login_proto_database.PlayerAllData{
		PlayerDatabaseData:   &login_proto_database.PlayerDatabase{PlayerId: playerId},
		PlayerDatabase_1Data: &login_proto_database.PlayerDatabase_1{PlayerId: playerId},
	}
}

// SeedResult 是 SeedNewPlayerAllData 的结论。零值 SeedError:漏判的分支落在最保守的「不知道写没写」上
// (写了也无害,见 seedNewPlayerScript)。
type SeedResult uint8

const (
	SeedError            SeedResult = iota // Redis 出错 / 回复不认识:种子是否落地未知
	SeedWritten                            // 父键原本不存在,已写入种子
	SeedIdempotent                         // 父键已是这份种子:go-redis 读超时后重发了已在服务端执行过的 EVAL
	SeedParentExists                       // 父键已存在且不是这份种子:未写
	SeedOwnerEpochExists                   // player:{id}:owner_epoch 已存在(玩家被派入过场景):未写
)

// String 返回指标 label 值(login_create_player_seed_total{result}),集合封闭。
func (r SeedResult) String() string {
	switch r {
	case SeedWritten:
		return "written"
	case SeedIdempotent:
		return "idempotent"
	case SeedParentExists:
		return "parent_exists"
	case SeedOwnerEpochExists:
		return "owner_epoch_exists"
	default:
		return "error"
	}
}

// seedNewPlayerScript 写新号种子。KEYS = {PlayerAllData 父键, player:{id}:owner_epoch};
// ARGV = {种子字节, TTL 毫秒}。
//
// 只在两件事同时成立时写:
//   - owner_epoch 键不存在。scene_manager 每次把玩家派入场景都会 INCR 它、从不删除(shared/ownerepoch 包注释;C++ redis_client.h kSaveIfGuardLuaScript 的注释)。
//     它在 = 这名玩家进过场景,scene 的存档才是真相,种子绝不能落。这一条兜住「种子请求在网络里滞留,
//     落地时玩家早已入场并存过盘、父键又恰好被删(合服失效缓存)」这类交错;
//   - 父键不存在。存在就不覆盖;若它恰好就是这份种子(go-redis 读超时后重发了已执行过的 EVAL),回 2 算成功。
//
// 写入带 PX,与冷读组装的父键同一 TTL(playerAllDataParentTTL);scene 首次存盘的裸 SET 会去掉 TTL。
// 两把键必须在同一实例:C++ 存盘的 owner_epoch 守卫脚本本来就同时访问它们(redis_client.h kSaveIfGuardLuaScript)。
//
// 返回:1 已写;2 已是这份种子;0 父键已存在且不同;-1 owner_epoch 已存在。
var seedNewPlayerScript = redis.NewScript(`
if redis.call("EXISTS", KEYS[2]) == 1 then
    return -1
end
local cur = redis.call("GET", KEYS[1])
if cur == false then
    redis.call("SET", KEYS[1], ARGV[1], "PX", ARGV[2])
    return 1
end
if cur == ARGV[1] then
    return 2
end
return 0
`)

// SeedNewPlayerAllData 在建角时写新号的 PlayerAllData 父键种子(内容 = newPlayerAllDataSkeleton),
// 让首次 EnterGame 的 EnsurePlayerAllDataInRedisAsync 命中快路径、不再发 Kafka 读任务。
//
// 前提(调用方保证):playerId 是刚铸出、尚未对任何人可见的号段 id。号段 id 不复用,所以此刻该 id 在任何库里
// 都没有行,种子就是真值。本函数只写缓存,不碰数据库;rc 必须与预加载、scene 读写父键是同一个 Redis 实例
// (既有契约,cross-zone-scene-travel.md CZ-2)。
//
// err 只在结论为 SeedError 时非 nil。
func SeedNewPlayerAllData(ctx context.Context, rc *redis.Client, playerId uint64) (SeedResult, error) {
	if rc == nil {
		return SeedError, fmt.Errorf("seed new player %d: redis client not wired", playerId)
	}
	if playerId == 0 {
		return SeedError, fmt.Errorf("seed new player: player_id is 0")
	}
	parent := newPlayerAllDataSkeleton(playerId)
	data, err := proto.Marshal(parent)
	if err != nil {
		return SeedError, fmt.Errorf("marshal seed for player %d: %w", playerId, err)
	}
	keys := []string{buildParentKey(parent, playerId), ownerepoch.OwnerEpochKey(playerId)}
	res, err := seedNewPlayerScript.Run(ctx, rc, keys, data, playerAllDataParentTTL.Milliseconds()).Int()
	if err != nil {
		return SeedError, fmt.Errorf("seed new player %d: %w", playerId, err)
	}
	switch res {
	case 1:
		return SeedWritten, nil
	case 2:
		return SeedIdempotent, nil
	case 0:
		return SeedParentExists, nil
	case -1:
		return SeedOwnerEpochExists, nil
	default:
		return SeedError, fmt.Errorf("seed new player %d: unexpected script result %d", playerId, res)
	}
}
```

几点说明:

- **参数用 `*redis.Client`,不用 `redis.Cmdable`**。仓内 `Script.Run` 的调用方全部传 `*redis.Client`(`loginlogic.go:331`、`queue.go:116`),照抄先例,不依赖接口满足关系。
- **import 写法**:沿用 dataloader 现有文件的分组,标准库和本仓路径同一组、按路径排序,第三方另起一组。
- **依赖都已在**:`shared/ownerepoch` 只依赖标准库(`ownerepoch.go:31`);`shared` 已经 replace 进 login(`go/login/go.mod:23`、:119)。
- **`playerAllDataParentTTL.Milliseconds()`**:它是 `time.Duration` 类型的常量,可以直接调方法,值为 86400000。

### B2.2 修改 `ensure_player_all_data_async.go`

1. 删除 import 第 8 行:`login_proto_database "proto/common/database"`。
2. 把 :49-52 替换为:

```go
	// 骨架与建角写的新号种子同源(newPlayerAllDataSkeleton):子表读空时父键就是它。
	parent := newPlayerAllDataSkeleton(playerId)
```

其余一字不改。

### B2.3 新增 `go/login/internal/logic/clientplayerlogin/createplayer_seed.go`

```go
package clientplayerloginlogic

import (
	"context"
	"login/internal/logic/pkg/dataloader"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

// newPlayerSeedTimeout 是种子写的 ctx 预算。理由同 accountBlobReadBackTimeout:它**封不住单次调用**
// (login 的 Redis 客户端没开 ContextTimeoutEnabled,单次 socket 读仍可等满 go-redis 默认的 3s),
// 真正限制的是 go-redis 的内部重发(默认 MaxRetries=3)。种子只是优化,不值得在账号锁里多等几轮。
const newPlayerSeedTimeout = time.Second

// seedNewPlayer 是 CreatePlayer 的 6d' 步:写新号的 PlayerAllData 父键种子
// (dataloader.SeedNewPlayerAllData;docs/design/new-player-first-enter-no-cold-read.md)。
//
// 尽力而为,**不返回错误、不改变建角结果**:种子是首次入场的缓存优化,不是任何数据的真源。没写成时,
// 首次 EnterGame 照旧冷读数据库,读到空就是真值(号段 id 不复用)。
//
// parent_exists / owner_epoch_exists 对新铸的 id 不该出现。出现即说明这个 id 以前被用过(发号器把在役 id
// 又发了一次,或 id_segment 表被重置而 Redis 没清),与名字登记的 FailedPrecondition 同属 ID 安全事件:
// 记 ERROR 并计数,但不在这里拒绝建角 —— 走到这一步 player:zone 已经登记,此时拒绝会留下永久的幽灵映射
// (见 6d 的注释)。是否升级为拒绝待负责人拍板(设计 §10 P2)。
func (l *CreatePlayerLogic) seedNewPlayer(account string, playerID uint64) {
	ctx, cancel := context.WithTimeout(l.ctx, newPlayerSeedTimeout)
	defer cancel()
	result, err := dataloader.SeedNewPlayerAllData(ctx, l.svcCtx.RedisClient, playerID)
	createPlayerSeedTotal.Inc(result.String())
	switch result {
	case dataloader.SeedWritten, dataloader.SeedIdempotent:
	case dataloader.SeedParentExists, dataloader.SeedOwnerEpochExists:
		// 这里首次入场**不一定**冷读:父键已在时走快路径,读到的是那份已有缓存(B4)。
		logx.Errorf("[new-player-seed] freshly minted player_id=%d already has %s — id reuse? "+
			"seed not written; first EnterGame uses the existing cache, or the DB read if there is none (account=%s)",
			playerID, result, account)
	default:
		logx.Errorf("[new-player-seed] seed not written, first EnterGame falls back to the DB read "+
			"(player_id=%d account=%s): %v", playerID, account, err)
	}
}
```

### B2.4 修改 `createplayerlogic.go`

**插入位置**:在下面这段之后、`// 6e. Name 是登记表的只读副本` 之前,也就是 :274 和 :276 之间。

```go
	tip = l.registerHomeZone(account, newPlayerId)
	stageDone()
	if tip != nil {
		l.releaseName(account, newPlayerId, name, false)
		resp.ErrorMessage = tip
		return resp, nil
	}
```

**插入内容**:

```go
	// 6d'. 新号父键种子(NewPlayerSeed.Enabled,默认关;docs/design/new-player-first-enter-no-cold-read.md)。
	// 排在 player:zone 登记之后:名字、归属两步 fail-closed,失败的建角不留种子。排在账号 blob 之前:
	// EnterGame 只认账号 blob 或反查索引(7 / 7b 才写),所以种子落地之前没有任何人能拿这个 id 进场,
	// 首次 EnterGame 看到的父键只可能是这份种子,或者缺席(缺席 = 走冷读老路,结果相同)。
	// 尽力而为:种子没写成不影响建角,结论只进日志与计数(seedNewPlayer)。
	if config.AppConfig.NewPlayerSeed.Enabled {
		stageDone = observeStage(createPlayerStageSeconds, createStageSeed)
		l.seedNewPlayer(account, newPlayerId)
		stageDone()
	}
```

`stageDone` 在 :223 已经用 `:=` 声明过,这里用 `=`。`config` 已经 import 过了。

### B2.5 修改 `clientplayerlogin/metrics.go`

1. **import**(:24-30)改为:

```go
import (
	"time"

	"github.com/zeromicro/go-zero/core/metric"

	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/sessionmanager"
)
```

2. **stage 注释和常量**(:237-245):
   - 注释「建角持着账号锁串行跑这四步」改为「建角持着账号锁串行跑这几步(开 NewPlayerSeed 时多一步 seed)」。
   - 在 `createStageRegister` 和 `createStageAccountWrite` 之间加一行:

```go
	createStageSeed         = "seed"          // 新号父键种子(NewPlayerSeed.Enabled 时才有,一次 Redis)
```

3. **Help 文案**:`createPlayerStageSeconds` 的 Help 改为 `"... Stage: mint | name | register | seed | account_write."`。

4. **新增计数器**:放在 `createPlayerNameOrphanTotal` 定义之后,同一个 `var ( … )` 块里。

```go
	// 新号种子的每一次写入记一条(只在 NewPlayerSeed.Enabled 时有)。result 取 dataloader.SeedResult.String()。
	// parent_exists / owner_epoch_exists 对新铸 id 稳态恒为 0:每 +1 都对应一条带 player_id 的 ERROR 日志
	// (`[new-player-seed] freshly minted ...`),意味着 id 复用,按 ID 安全事件处理。error 只表示种子没写成,
	// 首次入场退回冷读,不影响正确性。
	createPlayerSeedTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: createPlayerMetricNamespace,
		Subsystem: createPlayerMetricSubsystem,
		Name:      "seed_total",
		Help: "New-player PlayerAllData seed writes by CreatePlayer (NewPlayerSeed.Enabled). " +
			"Result: written | idempotent | parent_exists | owner_epoch_exists | error. " +
			"parent_exists / owner_epoch_exists on a freshly minted id mean id reuse (see the ERROR log).",
		Labels: []string{"result"},
	})
```

5. **`PrimeCreatePlayerMetrics`**(:303):
   - 函数体加两行:

```go
	// 种子的两个 id 复用结局同样用于「出现即告警」,理由同上。
	createPlayerSeedTotal.Add(0, dataloader.SeedParentExists.String())
	createPlayerSeedTotal.Add(0, dataloader.SeedOwnerEpochExists.String())
```

   - 函数前注释的首句「把孤儿计数器预置成 0,让这条序列从起服起就存在」改为「把孤儿计数器、以及新号种子的两个 id 复用结局预置成 0,让这几条序列从起服起就存在」。
   - 末句「只预置孤儿这一条…」改为「只预置孤儿与种子的两个 id 复用结局:name_release_total 带 label、且不用于『出现即告警』,不值得为它预铺四条序列」。
   - 开关关着时也会预置这两条 0 序列:无害,告警表达式(`increase(...) > 0`)照样恒为假,不必按开关区分。

### B2.6 修改 `player_class_backfill.go`(只改注释 :18-20)

```go
// backfillPlayerIdentity 只在首次入场前补齐存档里缺失的角色身份:旧存档漏存的职业,
// 以及角色名副本 PlayerProfileComp.name。建角不写身份:PlayerAllData 要么由首次 EnterGame 的
// 预加载建出,要么是建角写的种子(NewPlayerSeed,只带 player_id,与冷读读空的结果相同),
// 两种情况下名字都只能和职业走同一条路、同一次整字节 CAS 写进去。
```

:21-24 的其余注释不动。

### B2.7 修改 `go/login/internal/config/config.go`

**1. Config 字段**:在 :103 `Placement PlacementConf …` 之后插入。

```go

	// NewPlayerSeed 控制建角时是否写「新号父键种子」,让新号首次 EnterGame 不再冷读数据库
	// (docs/design/new-player-first-enter-no-cold-read.md)。整块可缺省,缺省 = 关 = 与本功能之前逐字节相同。
	NewPlayerSeed NewPlayerSeedConf `json:"NewPlayerSeed,optional"`
```

**2. 类型**:在 `StorageIDForNewPlayer` 函数结束(:188)之后插入。

```go

// NewPlayerSeedConf 是「新号首次入场免冷读」的开关(docs/design/new-player-first-enter-no-cold-read.md)。
//
// 正向命名,零值 = 关:go-zero 不给整块缺失的 optional 结构体递归填内层 default(见 HomeZoneConf 的说明),
// 所以默认值只能靠零值表达。
type NewPlayerSeedConf struct {
	// Enabled=true:CreatePlayer 在落账号 blob 之前写 PlayerAllData:{id} 种子(两张子表只带 player_id,
	// TTL 与冷读组装的父键相同),新号首次 EnterGame 命中缓存直接进场。种子没写成不影响建角(退回冷读)。
	// 打开前置:go/db 的 db_placement_guard_total{outcome="missing_required"} 与
	// db_placement_store_open_total{result=~"error|rejected"} 两类告警已就位(设计 §9 第 2 条)。
	Enabled bool `json:"Enabled,optional"`
}
```

**3. `PlacementConf` 注释**:在 :165-166「进游戏时预加载秒级失败…」「要关这些,先把全部 go/db 的 Required 关掉。」之后、`type PlacementConf struct` 之前插入两行(按文本定位,不要按行号,上面第 1 处插入会让行号后移):

```go
// 开着 NewPlayerSeed 时新号首次入场不走预加载读,「预加载秒级失败」不会出现,只剩存盘这一半
// (死信 / 无限重试),更隐蔽(docs/design/new-player-first-enter-no-cold-read.md §9 第 2 条)。
```

### B2.8 修改 `go/login/internal/svc/home_zone.go`

在 :39-40 的 `[placement]` 日志之后,函数结束之前插入:

```go
	// 新号种子打开时,§13 第 3 条违例的症状从「新号预加载秒级失败」变成「进得去、存盘全进死信」,更隐蔽
	// (docs/design/new-player-first-enter-no-cold-read.md §9)。与 pin_on_create 打在一起,方便核对。
	logx.Infof("[new-player-seed] enabled=%v (pin_on_create=%v)", config.AppConfig.NewPlayerSeed.Enabled, pl.PinOnCreate)
```

注意:`DataServiceClient==nil` 时函数在 :27-31 提前返回,这一行不会打印。这种情况下建角本身就会被拒绝,所以可以接受。

### B2.9 修改 `go/login/etc/login.yaml`

**1. Locker 注释**:在 :68-69 那一条(「三次 Redis … 三步合计 < 9s。」)之后加三行。:70「20 盖住…」那句原样保留,由新加的这三行说明它在开关打开时不再成立:

```yaml
  #   - 开 NewPlayerSeed 时多一次 Redis(新号种子;代码里的 1s ctx 预算只掐重发,单次最坏仍是 3s 读超时):
  #     Redis 合计变成四步 < 12s,加上 gRPC 的 9s 最坏是 21s,**超过**下面的 20 —— 开着种子时「20 盖住」
  #     不再成立,正确性照旧只靠围栏 + 回读;是否上调见 docs/design/new-player-first-enter-no-cold-read.md §10 P6。
```

**2. Placement 注释第 3 条**:在 :231 之后加一行。

```yaml
#      开着 NewPlayerSeed 时新号不再走预加载读,违反本条的症状只剩存盘这一半(死信 / 无限重试),更隐蔽(见下方 NewPlayerSeed)。
```

**3. 新块**:插在 :235 之后(`NewPlayerStorageId: 0` 下方空行处)。

```yaml

# ─────────────────────────────────────────────────────────────────────────
# 新号首次入场免冷读(docs/design/new-player-first-enter-no-cold-read.md)
#
# Enabled:true = CreatePlayer 在落账号 blob 之前写一份 PlayerAllData:{id} 种子(两张子表只带 player_id,
#   与「冷读读空后 login 组装出的父键」相同,TTL 24h),新号首次 EnterGame 命中缓存,不再发 Kafka 读任务、
#   不再同步等 go/db(现状单任务最长 8s)。false(默认,整块不写也一样)= 与本功能之前完全一致。
#   种子只是缓存:写失败 / 过期 / 被删时首次入场退回冷读,结果相同;它永不覆盖已有父键,
#   也不会写给进过场景的玩家(player:{id}:owner_epoch 已存在)。
#   前提(既有契约,不是新增):PlayerAllData 与 player:{id}:owner_epoch 在同一个 Redis 实例(CZ-2 / CZ-4)。
#   打开前置:go/db 的 db_placement_guard_total{outcome="missing_required"} 与
#   db_placement_store_open_total{result=~"error|rejected"} 两类告警已就位(见上方 Placement 第 3 条)。
#   开着时新号的入场链在 PreloadPool 里跑完,池满回 kLoginInProgress;压测看 [preload-pool] 日志的 dropped_delta。
#   启动日志 "[new-player-seed] enabled=..." 是核对依据。
NewPlayerSeed:
  Enabled: false
```

### B2.10 修改 `deploy/login-stack.linux/login.yaml`

**1. Placement 注释第 3 条**:在 :150 之后加一行,与 B2.9 第 2 处同文:

```yaml
#      开着 NewPlayerSeed 时新号不再走预加载读,违反本条的症状只剩存盘这一半(死信 / 无限重试),更隐蔽(见下方 NewPlayerSeed)。
```

**2. 新块**:在 :154 `NewPlayerStorageId: 0` 之后、空行之前插入(行号按插入第 1 处之前算;按文本定位):

```yaml

# 新号首次入场免冷读(docs/design/new-player-first-enter-no-cold-read.md);默认关,打开前置见设计 §7。
NewPlayerSeed:
  Enabled: false
```

### B2.11 修改 `docs/design/player-storage-placement.md:448`(推荐做)

在这一段末尾追加一句,不改原句:

> 开着 login 的 `NewPlayerSeed` 时,新号首次进场不再走预加载读,「预加载秒级失败」不会出现,症状只剩存盘这一半:死信(`missing_required`),或落点库打不开时的无限重试。所以打开种子前,`missing_required` 与 `db_placement_store_open_total{result=~"error|rejected"}` 两类告警必须就位([new-player-first-enter-no-cold-read.md](../../design/new-player-first-enter-no-cold-read.md) §9)。

## B3 关键类型 / 函数 / 脚本 / 配置键 / 指标

| 类别 | 名字 | 说明 |
|---|---|---|
| 函数 | `dataloader.newPlayerAllDataSkeleton(playerId uint64) *PlayerAllData` | 冷读和种子共用的骨架 |
| 函数 | `dataloader.SeedNewPlayerAllData(ctx, rc *redis.Client, playerId uint64) (SeedResult, error)` | 写种子 |
| 类型 | `dataloader.SeedResult` | Error(0) / Written / Idempotent / ParentExists / OwnerEpochExists;`String()` 给出 label |
| 脚本 | `seedNewPlayerScript` | KEYS 为 `PlayerAllData:{id}`、`player:{id}:owner_epoch`;ARGV 为种子字节、`86400000`;返回 1 / 2 / 0 / -1 |
| 方法 | `(*CreatePlayerLogic).seedNewPlayer(account string, playerID uint64)` | 无返回值 |
| 常量 | `newPlayerSeedTimeout = 1s` | — |
| 常量 | `createStageSeed = "seed"` | — |
| 配置 | `NewPlayerSeed.Enabled` | bool,缺省 false;整块缺失也是 false |
| 指标 | `login_create_player_seed_total{result}` | 新增 |
| 指标 | `login_create_player_stage_seconds{stage="seed"}` | 新 label 值 |
| 日志 | `[new-player-seed] freshly minted player_id=… already has …`(ERROR) | id 复用 |
| 日志 | `[new-player-seed] seed not written …`(ERROR) | 写失败 |
| 日志 | `[new-player-seed] enabled=… (pin_on_create=…)`(INFO,启动时) | — |
| SQL | 无 | — |
| Redis 键 | 不新增 | 复用 `PlayerAllData:{id}`;只读 `player:{id}:owner_epoch` |
| proto | 不改 | — |

## B4 错误语义

- **`CreatePlayer` 的返回不变**,不论种子结果如何:
  - tip、`resp.Players`、gRPC error 都和关闭开关时一样;
  - 已有的 fail-closed 步骤(发号、名字、归属、账号 blob)不变。
- **`seedNewPlayer` 各结局**:

| 结局 | 处理 | 首次进场 |
|---|---|---|
| written / idempotent | 只计数 | 快路径 |
| parent_exists / owner_epoch_exists | ERROR 日志(带 player_id、account)+ 计数 | 已有的父键在:快路径,读到的是已有数据,和今天一样。只有 owner_epoch 在、父键不在:冷读 |
| error(Redis 错误、ctx 超时 / 取消、脚本回复不认识、nil 客户端) | ERROR 日志 + 计数 | 冷读 |

- **口令不进日志**:日志里只有 player_id、account 和 Redis 错误串。go-redis 的错误不带口令。
- **指标 label 只有结局枚举**,没有 player_id(AGENTS §9)。

## B5 测试清单

所有用例都在进程内跑,只用 miniredis,不需要外部依赖。

### B5.1 `go/login/internal/logic/pkg/dataloader/new_player_seed_test.go`(package dataloader)

| 用例 | 断言 |
|---|---|
| `TestSeedNewPlayerAllData_WritesSkeletonWithTTL` | 返回 `SeedWritten`;GET 父键后反序列化,与 `newPlayerAllDataSkeleton(id)` `proto.Equal`;`ProfileComponent`、`Uint32PbComponent` 为 nil;`registration_timestamp==0`;`mr.TTL(key)` 落在 (0, 24h] |
| `TestSeedNewPlayerAllData_RerunIsIdempotent` | 连续调两次,依次得到 `SeedWritten`、`SeedIdempotent`;值不变 |
| `TestSeedNewPlayerAllData_NeverOverwritesExistingParent` | 预置父键 = 带 `LevelComponent{Level:30}` 的 blob,不设 TTL;返回 `SeedParentExists`;字节不变;`mr.TTL==0`(没有被加上 TTL) |
| `TestSeedNewPlayerAllData_RefusesOnceOwnerEpochExists` | 预置 `ownerepoch.OwnerEpochKey(id)="3"`、父键不在;返回 `SeedOwnerEpochExists`;父键仍不存在 |
| `TestSeedNewPlayerAllData_RejectsZeroIDAndNilClient` | `playerId=0` 和 `rc=nil` 都返回 `SeedError` + 非 nil err;不碰 Redis |
| `TestSeedNewPlayerAllData_RedisDownIsError` | `mr.SetError("simulated outage")`,`t.Cleanup` 里 `mr.SetError("")`(先例:`createplayer_name_test.go` 的 `TestCreatePlayerName_WriteAndReadBackBothFailKeepsReservation`)。返回 `SeedError` + 非 nil err;清掉错误后父键不存在。不用 `mr.Close()`:关掉后 go-redis 要走拨号重试,耗时取决于退避,不如注入错误确定 |
| `TestSeedEqualsColdReadAssemblyForEmptyRows` | 实例 1 调 `SeedNewPlayerAllData(id)`。实例 2 预置子表缓存 `player_database:{id}` = `Marshal(PlayerDatabase{PlayerId:id})`、`player_database_1:{id}` = `Marshal(PlayerDatabase_1{PlayerId:id})`(正是 go/db 读空时回写的内容),再调 `EnsurePlayerAllDataInRedisAsync(ctx, rdb2, nil, nil, id, 0, cb)`,走 :102-109 的无任务分支。断言回调得到 nil、两边父键反序列化后 `proto.Equal`、两边都有 TTL。键名用 `Descriptor().FullName()` 拼,不手写 |
| `TestEnsurePlayerAllData_SeededParentTakesFastPath` | 先写种子,再调 `EnsurePlayerAllDataInRedisAsync(ctx, rdb, nil, nil, id, 0, cb)`。断言回调**同步**被调用恰好一次且 err 为 nil(disp / producer 为 nil,快路径之外的任何分支都会 panic);两个子表缓存键都不存在;不存在 `task:result:*` 键 |

### B5.2 `go/login/internal/logic/clientplayerlogin/createplayer_seed_test.go`(package clientplayerloginlogic)

复用同包已有的 `newCreatePlayerHarnessWithNames`、`fakeRegisterClient`、`hzPlayerID`、`hzAccount`、`storedPlayers`、`createPlayerCounterValue`、`newNameHarness`、`nameCreateKey`、`requireTip`、`newEnterWiringHarness`、`identityPlayerID`、`identityBlobKey`、`readIdentityBlob`。不新增假客户端类型。

- **开关**:新增 helper `enableNewPlayerSeed(t)`,保存并 `t.Cleanup` 恢复 `config.AppConfig.NewPlayerSeed`。
- **计数**:新增 helper `seedCount(t, result)`,即 `createPlayerCounterValue(t, "login_create_player_seed_total", {"result": result})`。
  - 动作**之前**先取一次基线:helper 内部会 `gzprom.Enable()`,单测里指标开关默认是关的。
  - 断言用差值。

| 用例 | 断言 |
|---|---|
| `TestCreatePlayer_SeedOffWritesNoParentKey` | 显式设 `Enabled=false`;建角成功;`h.mr.Exists(PlayerAllData:{hzPlayerID})` 为 false |
| `TestCreatePlayer_SeedOnWritesSkeletonBeforeAccountBlob` | 开开关;建角成功;父键 = 骨架(`proto.Equal`);TTL 在 (0, 24h];账号 blob 含新角色;`seedCount("written")` +1;`fake.events` 仍是 `"reserve,register"`(种子不碰 data_service) |
| `TestCreatePlayer_SeedSkippedWhenHomeZoneRegistrationFails` | 开开关;`fakeRegisterClient{err: errors.New("down")}`;建角失败;父键不存在 |
| `TestCreatePlayer_SeedSurvivesLostAccountWrite` | 开开关。用 `n := newNameHarness(t)`,照 `TestCreatePlayerName_LostLockFencesAccountWrite` 的做法,在 `n.fake.reserve` 里 `n.rdb.Del(n.ctx, nameCreateKey)` 删掉建角锁,再回 `ReserveOK`;`resp := n.create(t, "云中君")`。断言:`requireTip(t, resp, kLoginInProgress)`(围栏写回 -1、回读不含);`storedPlayers` 为空;父键存在且 TTL > 0;`len(n.fake.releaseReqs)==1`、`len(n.scheduled)==0`。**证明种子写在账号 blob 之前,失败的建角只留下会过期的种子** |
| `TestCreatePlayer_SeedNeverOverwritesExistingParent` | 开开关;预置父键(等级 30 的 blob,不设 TTL);建角成功(tip nil);父键字节不变、TTL 仍为 0;`seedCount("parent_exists")` +1 |
| `TestCreatePlayer_SeedRefusedAfterOwnerEpochMinted` | 开开关;预置 `player:{hzPlayerID}:owner_epoch="3"`;建角成功;父键不存在;`seedCount("owner_epoch_exists")` +1 |
| `TestCreatePlayer_SeedErrorDoesNotFailCreate` | 开开关;把父键预置成 list(`h.mr.Lpush(父键, "x")`),种子脚本里的 `GET` 会 WRONGTYPE 报错,`EVAL` 整体返回错误;别的键不受影响。断言:建角成功(tip nil);账号 blob 含新角色;反查索引在;`seedCount("error")` +1;`h.mr.Type(父键)=="list"`(没被覆盖)。**证明 B4「种子结果不改变建角结果」对 error 分支同样成立** |
| `TestEnterGame_SeededNewPlayerTakesFastPath` | `h := newEnterWiringHarness(t, []{{PlayerId: identityPlayerID, ClassId: 3, Name: "云中君"}}, nil)`。先 `h.rdb.Del(ctx, identityBlobKey())`,再 `dataloader.SeedNewPlayerAllData(ctx, h.rdb, identityPlayerID)`,要求返回 `SeedWritten`。然后 `EnterGame` + `waitChain`。断言:`setSessionCalls==1`(链过了身份补齐;svc 里 KafkaClient / TaskResultDispatcher 为 nil,任何冷读分支都会 panic,链就走不到这里);存档职业=3、名字=云中君;`PlayerDatabase_1Data.PlayerId==identityPlayerID`;`h.rdb.PTTL(父键)` 落在 (0, 24h](KEEPTTL 保留了种子 TTL);两个子表缓存键都不存在 |

### B5.3 `go/login/internal/config/new_player_seed_conf_test.go`(package config)

仿 `placement_conf_test.go`。

| 用例 | 断言 |
|---|---|
| `TestNewPlayerSeedBlockMissingMeansOff` | `conf.LoadFromYamlBytes("Name: login.rpc\nListenOn: 127.0.0.1:1\n")` 之后,`c.NewPlayerSeed.Enabled==false`(加载错误忽略,理由同 Placement 用例) |
| `TestLoginYamlSpellsOutNewPlayerSeedKeys` | 用不带 optional 的探针 `newPlayerSeedKeysProbe{NewPlayerSeed struct{Enabled bool \`json:"Enabled"\`} \`json:"NewPlayerSeed"\`}` 加载 `../../etc/login.yaml` 和 `../../../../deploy/login-stack.linux/login.yaml`,都必须成功 |
| `TestNewPlayerSeedKeysProbeRejectsTypos` | 段名拼错(`NewPlayerSed:`)、键名拼错(`Enable:`)、整块缺失,三种都必须加载失败;拼写正确的块必须加载成功 |
| `TestEtcYamlNewPlayerSeedIsOff` | `conf.Load("../../etc/login.yaml", &c)` 之后 `c.NewPlayerSeed.Enabled==false` |

### B5.4 回归(已有用例,不改)

- `createplayer_homezone_test.go`、`createplayer_name_test.go`、`createplayer_idpath_test.go`、`player_identity_backfill_test.go`:全部照旧通过。开关默认关,行为逐字节不变。
- `placement_conf_test.go`:照旧通过。yaml 新块和 Placement 块互不影响。

## B6 交 Codex 的验证命令与通过标准

- **工作目录**:仓库根 `D:\luyuan\xuanming-server-mmo-cpp`。
- **环境**:不需要环境变量、外部服务和前置清理;单测全用 miniredis。
- **执行**:命令串行执行。

```powershell
cd go/login

# 1) 格式:本批新增文件必须零输出;修改的既有文件,gofmt -d 的差异只允许是 HEAD 既有的(本批改动行不得出现)
gofmt -l internal/logic/pkg/dataloader/new_player_seed.go internal/logic/pkg/dataloader/new_player_seed_test.go internal/logic/clientplayerlogin/createplayer_seed.go internal/logic/clientplayerlogin/createplayer_seed_test.go internal/config/new_player_seed_conf_test.go
gofmt -d internal/logic/pkg/dataloader/ensure_player_all_data_async.go internal/logic/clientplayerlogin/createplayerlogic.go internal/logic/clientplayerlogin/metrics.go internal/logic/clientplayerlogin/player_class_backfill.go internal/config/config.go internal/svc/home_zone.go

# 2) 静态检查与编译
go vet ./internal/logic/pkg/dataloader/... ./internal/logic/clientplayerlogin/... ./internal/config/... ./internal/svc/...
go build ./...

# 3) 定向测试
go test ./internal/logic/pkg/dataloader/... -run "Seed|Skeleton|SeededParent" -count=1 -v
go test ./internal/config/... -run "NewPlayerSeed|Placement|EtcYaml|LoginYaml" -count=1 -v
go test ./internal/logic/clientplayerlogin/... -run "Seed|CreatePlayer|RegisterHomeZone|EnterGame|BackfillPlayerIdentity" -count=1 -v

# 4) 全量
go test ./... -count=1

# 4b) 竞态(可选)。docs/PROGRESS.md 记录本机没有 gcc,cgo 起不来,-race 无法编译。
#     本机没有 gcc 就跳过这一条,在结果里如实写「-race 未跑:无 gcc」。
#     不要为它安装 gcc 或起容器(AGENTS §10.2),除非用户另行授权。
go test -race ./internal/logic/pkg/dataloader/... ./internal/logic/clientplayerlogin/... -count=1

# 5) 文档索引(仓库根)
cd ../..
python tools/scripts/gen_docs_index.py --check
```

### 通过标准

- 第 1 步:第一条命令零输出;第二条的 diff 里没有本批改动的行。
- 第 2 步:`go vet` 和 `go build` 零报错。
- 第 3 步:
  - B5.1 的 8 个、B5.2 的 8 个、B5.3 的 4 个新用例全部 **PASS**,不得 SKIP;
  - 已有的 Placement、CreatePlayer、EnterGame、Backfill 用例全部 PASS。
- 第 4 步:`go test ./...` 全绿。
- 第 4b 步:跑了就必须无 DATA RACE;没跑就写明「未跑 + 原因」,不算通过也不算失败,列为未验证项。
- 第 5 步:退出码 0。

### 失败时保留

- 失败用例的完整 `-v` 输出;
- `go vet` / `go build` 的报错原文;
- `gen_docs_index.py --check` 的差异输出。

不要连续自动重试。

### 重点核对(凭记忆写的、本机无法验证的点)

- miniredis v2.35 在 Lua 里执行 `SET key val PX ms` 会设上 TTL,`GET` 缺键返回 false。如果 `TestSeedNewPlayerAllData_WritesSkeletonWithTTL` 的 TTL 断言失败,先查这一点。
- `seedNewPlayerScript.Run(ctx, *redis.Client, …)` 的 EVALSHA → NOSCRIPT → EVAL 回退,在 miniredis 上能走通(`player_identity_backfill_test.go` 已有先例)。
- `metric.CounterVec` 的 `Add(0, label)` 签名:同文件里 `createPlayerNameOrphanTotal.Add(0)` 是先例。
- miniredis 在 Lua 里对 list 执行 `redis.call("GET", …)` 会抛 WRONGTYPE,`EVAL` 整体回错误。`TestCreatePlayer_SeedErrorDoesNotFailCreate` 依赖这一点;如果它没走到 `error` 分支,先查这一点。

> **核对结论(2026-10-15;正文不改)**:本机模块缓存有与 `go/login/go.mod` 同版本的 miniredis v2.35.0(`:7`)、go-redis v9.16.0(`:13`)、go-zero v1.9.2(`:15`)和 miniredis 用的 gopher-lua v1.1.1(`:79`,indirect),目录 `C:\Users\luyua\go\pkg\mod\github.com\alicebob\miniredis\v2@v2.35.0`、`…\github.com\redis\go-redis\v9@v9.16.0`、`…\github.com\zeromicro\go-zero@v1.9.2`、`…\github.com\yuin\gopher-lua@v1.1.1`,下文路径相对各自目录。对照落地后的脚本 `go/login/internal/logic/pkg/dataloader/new_player_seed.go:74-87` 与调用 `:110`,上面四条**都与源码一致**,用例依赖的假设成立。**这是只读源码的静态核对,不是运行证据**:用例过没过仍以 Codex 按本节第 3 步跑出的结果为准。
> - **Lua 里 `SET key val PX ms` 设上 TTL**:成立。go-redis 把种子 `[]byte` 原样编成 bulk、把 `int64` 编成十进制字符串(`internal/proto/writer.go:73-74`、`:103-104`、`:227-229`);miniredis 把 ARGV 作为 Lua 字符串放进表(`cmd_scripting.go:82-86`),`redis.call` 把字符串参数原样交给 `Dispatch`(`lua.go:43-44`、`:57-59`);`cmdSet` 的 `PX` 分支按毫秒解析(`cmd_string.go:84-86`、`:99-114`,≤ 0 报错 `:105-109`),先删旧值连同旧 TTL(`:171`)再写值(`:174`)并写 `db.ttl`(`:176-178`)。`mr.TTL` 直接返回 `db.ttl[k]`(`direct.go:401-413`),不随墙钟递减(spec_G4b-G.md §13 U2),所以 `new_player_seed_test.go:106` 与 `createplayer_seed_test.go:125` 的 `(0, 24h]` 断言拿到的正好是 24h。
> - **`GET` 缺键在 Lua 里是 false**:成立。`cmdGet` 缺键写 null(`cmd_string.go:402-404`);`redis.call` 用的是新建的 peer(`lua.go:57`),`server.NewPeer` 不设 `Resp3`(`server/server.go:259-263`、`:293-297`),null 按 RESP2 写成 `$-1`(`server/server.go:464-470`),`ParseReply` 解成 nil(`server/proto.go:124-127`),再转成 Lua `false`(`lua.go:78-79`)—— 与客户端连接是不是 RESP3 无关。脚本里 `cur == ARGV[1]` 的比较也成立:bulk 回复解成 Go string(`server/proto.go:128-139`)转 `LString`(`lua.go:92-93`),gopher-lua 的字符串相等是 Go 字符串逐字节相等(`vm.go:2421-2422`;类型不同直接不等,`:2407-2408`),`TestSeedNewPlayerAllData_RerunIsIdempotent`(`new_player_seed_test.go:112-125`)依赖的「第二次看到自己写的种子回 2」成立。
> - **`EVALSHA` → `NOSCRIPT` → `EVAL` 回退**:成立,与 spec_G4b-G.md §13 U4 同版本的结论相同。`Script.Run` 见 go-redis `script.go:66-74`;`HasErrorPrefix` 先 `errors.As` 成 `redis.Error`、去掉 `ERR ` 前缀再比前缀(`error.go:32-40`);miniredis 对没缓存的 sha 回 `NOSCRIPT No matching script. Please use EVAL.`(`cmd_scripting.go:202-206`,文案 `redis.go:58`)。`EVAL` 成功后按 sha 缓存脚本(`cmd_scripting.go:175-178`),同一 miniredis 上第二次调用直接由 `EVALSHA` 执行(`:209`)。
> - **`metric.CounterVec` 的 `Add(0, label)`**:成立。go-zero v1.9.2 `core/metric/counter.go:13-19` 的接口含 `Add(v float64, labels ...string)`(`:17`),实现在 `:49-53`;同包 `metrics.go:328-331` 已这样用。
> - **对 list 执行 `GET` 回 WRONGTYPE,`EVAL` 整体回 error**:成立,机理与 spec_G4b-G.md §13 U1 同版本的结论相同。非 string 键回 `msgWrongType`(`cmd_string.go:406-408`,文案 `redis.go:26`);错误回复被 `ParseReply` 解成 Go error(`server/proto.go:104-106`),`redis.call` 是 failFast 模式(`lua.go:106`),据此抛 Lua error(`lua.go:63-71`);`doScript` 把运行期错误也包成 error 返回(`cmd_scripting.go:129-132`),`runLuaScript` 写错误回复(`:106-108`),出错的脚本**不进缓存**(`:175-178`),所以这个用例里是 `EVALSHA` 先回 `NOSCRIPT`、`EVAL` 再回错误,两条路都落到 `SeedError`(go-redis `Cmd.Int()` 在有错时直接返回该错误,`command.go:316-318`;`new_player_seed.go:110-113`)。注意:错误文案是 `ERR Error compiling script (new function): …WRONGTYPE…`(`redis.go:78-80` 对运行期错误也套这个前缀),不以 `WRONGTYPE` 开头;`TestCreatePlayer_SeedErrorDoesNotFailCreate`(`createplayer_seed_test.go:239-264`)只断言 `seed_total{error}` +1 和 `mr.Type(key)=="list"`,不比文案,不受影响。脚本在 `GET` 处就出错,`SET` 不会执行,`h.mr.Lpush`(`direct.go:190-192`)预置的 list 原样保留,`mr.Type` 回 `"list"`(`direct.go:431-441`,`redis.go:18`)。
> - 顺带(不在上面四条里):`TestEnterGame_SeededNewPlayerTakesFastPath` 的 PTTL 断言(`createplayer_seed_test.go:306`)依赖身份补齐脚本的 `SET … KEEPTTL`(`player_class_backfill.go:190`)保留种子 TTL。miniredis `cmdSet` 的 `KEEPTTL` 先取旧 TTL、写入后写回(`cmd_string.go:80-83`、`:157-161`、`:171-178`),成立;Lua 内 `PTTL` 的实现见 spec_G4b-G.md §13 U15。
> - 以上只说明 miniredis / go-redis 的实现与用例假设一致;真实 Redis 上的同名行为不在本注范围(本节「可选冒烟」覆盖一部分)。

### 可选冒烟(需要本地全栈,不是压测,不下性能结论)

1. 按 AGENTS §6.2 清空 Redis / MySQL / etcd,重置 Kafka offset。
2. 把 `go/login/etc/login.yaml` 的 `NewPlayerSeed.Enabled` 临时改成 true。**不要提交这个改动。**
3. 用 `pwsh tools/scripts/dev_tools.ps1 -Command dev-start` 起全栈,让 robot 用 20 个新账号「建角 → 进场」。
4. 核对指标与日志:
   - login 的 `/metrics`(:9101):`login_create_player_seed_total{result="written"}` = 20;`dataloader_preload_cache_check_seconds_count{hit="miss"}` 不随这 20 次进场增长;
   - go/db 的 `/metrics`(:9160,AGENTS §6.1):`db_task_result_total{op="read"}` 不增长;
   - 20 个 robot 都进场成功,且之后各有一次存盘落库(go/db `db_task_result_total{op="write",result="ok"}` 增长);
   - login 日志的 `[preload-pool]` 行:`dropped_total` 不增长。
5. 把开关改回 false 再跑 20 个,确认 miss 计数和读任务计数恢复增长。
6. 结果写进 PROGRESS 的 Codex 条目,注明「功能冒烟,非压测」。

性能结论只能来自 L-14 的开服尖峰压测,按 AGENTS §6 开、关各跑一轮,用 `stress_summarize` 出对比表,并注明开关状态。

## B7 文档登记

1. **新文档**:`docs/design/new-player-first-enter-no-cold-read.md` = Part A。首行标题是「# 新号首次进场免冷读:建角时写入 PlayerAllData 种子」;标题里不带状态,状态写在头部引用块里,以后改状态不用改索引行。
2. **`docs/README.md`**:「#### 玩家数据与存储」分类按文件名排序,插在 `global-data-layer-tidb-decision.md` 那行(工作区约 :139)之后、`player-async-save-loss-windows.md` 那行之前:
   ```
   - [new-player-first-enter-no-cold-read.md](design/new-player-first-enter-no-cold-read.md) — 新号首次进场免冷读:建角时写入 PlayerAllData 种子
   ```
3. **`tools/scripts/gen_docs_index.py`**:「玩家数据与存储」规则(工作区 :38-43)在 `"player-route-mapping-*",` 之后加一行:
   ```python
        "new-player-*",
   ```
   前面的分类都不会命中 `new-player-*`,docs/design 里也没有其他 `new-player` 开头的文件。
   - 如果 Docs 阶段同时登记别的文档,行序以生成器输出为准;`--check` 交给 Codex 跑。
4. **Docs 阶段统一做,本阶段和实现阶段都不做**:
   - `docs/PROGRESS.md` 追加一条:注明未编译,Java 版本号「无」,状态「不适用」;
   - `tidb-phase1-and-launch-readiness.md` 的 L-05 标「设计已出 / 已落码未验证」;
   - Java `PARITY.md` 登记一行「新号首次进场免冷读 | 不适用(Java 建角同步插行、进场按主键读本库)」。

## B8 与现有设计的偏离与代价

见 Part A §9,共 7 条。实现层面再补三点:

- **骨架抽成共享函数**:`ensure_player_all_data_async.go` 的骨架改成调用共享函数。这是为了满足「种子 ≡ 冷读结果」这条不变量(DRY),不算顺手重构。
- **启动日志放在 `initHomeZoneResolver` 里**:和 `[placement]` 日志相邻,方便一起核对。代价是 DataServiceRpc 没配时不打印,但那时建角本来就会被拒。
- **K8s 不在本批打开**:`k8s_deploy.ps1` 的 login ConfigMap 不写这一键,所以 K8s 上整块缺失 = 关。打开要另起一批,同步改契约测试。

## B9 风险

见 Part A §10 的 R1–R8。实现层面再补三点:

1. **CreatePlayer 用例共用全局 `config.AppConfig`**。新用例必须 `t.Cleanup` 恢复 `NewPlayerSeed`,否则会污染同包后续用例的默认行为。
2. **计数器是进程级全局**。用例只能断言差值,而且要先取一次值,让 `gzprom.Enable()` 生效。
3. **`TestCreatePlayer_SeedSurvivesLostAccountWrite` 依赖 `writeAccountBlob` 现有的回读分支**(`createplayerlogic.go:621-656`)。如果那段逻辑以后改了,这个用例要同步改。

## B10 待负责人拍板

见 Part A §10 的 P1–P6。本规格按默认值落码:采用 A;id 复用只告警;K8s 不打开;不做 TTL 续期加固;`missing_required` 与 `db_placement_store_open_total{result=~"error|rejected"}` 两类告警作为打开前置;`AccountLockTTL` 不调。

## B11 Java 版(AGENTS §12)

**状态:不适用,对应 Java 版本号:无。**

- **依据**:
  - Java 建角在事务里插入玩家行(`D:\luyuan\xuanming-server-mmo\game-login\src\main\java\com\game\login\handler\CreatePlayerHandler.java:222`);
  - 进场按主键读本库(`EnterGameHandler.java:273` `store.findPlayer`);
  - scene 加载同样直读(`game-scene\src\main\java\com\game\scene\storage\StoragePlayerRepository.java:183-195`)。
  - 所以不存在「读不存在的行,还经 Kafka 同步等待」这条链。
- **契约**:本功能不改客户端可见的消息、tip 码和配表,不需要两版同批改。
- **登记**:Docs 阶段在 `PARITY.md` 登记「不适用」一行。

## B12 未核实项

1. **miniredis v2.35 的 Lua 行为**:`SET … PX` 设 TTL、`GET` 缺键返回 false、EVALSHA 回退。按经验应该成立,需要 Codex 跑用例确认。
   - 2026-10-15 结论注:已对本机模块缓存里同版本的 miniredis v2.35.0 / go-redis v9.16.0 源码逐条核过,这三点和 B6 列的「对 list 执行 `GET` 回 WRONGTYPE、`EVAL` 整体回错」都成立,文件:行依据见 B6「重点核对」后的核对结论注。「按经验应该成立」以该注为准;它是静态读源码,不是运行证据,仍需 Codex 跑用例确认。
2. **收益量级**:Part A §2.7 只是按代码数出的推算,没有计数和压测。要看 L-14。
3. **TTL 边界窗口**:「进场链耗时百毫秒级」是按链路结构推断的,没有实测分布。
4. **客户端行为**:「客户端 60s 进场超时兜底」取自 `entergamelogic.go:301` 的注释,没有核对 Unity 客户端代码(未授权读客户端仓库,AGENTS §9)。
5. **`player_lifecycle.cpp:3555` 的 `SNAPSHOT_LOGIN`**:新号首次进场照旧会发,对全球审计管道(L-07)的负载不变。G3 不减少这部分负载。
6. **入场链改占 PreloadPool 后的掉单量**:单次 apply 在开服尖峰下要多久、256 个 worker 够不够,没有数据。要看 L-14 的 `[preload-pool]` 日志与 `entergame_apply_*`。
7. **`-race`**:本机没有 gcc,大概率跑不了(B6 第 4b 步)。

---

# 评审处理记录(对抗式评审,2026-10-12)

评审方式:逐条回到代码核对,基准是 HEAD `73b852f32` 加工作区。只读文件、grep、git 只读命令,没有编译、测试或运行任何命令;仓库文件一个没改,只改了本规格。

## 必须改(1 条,已吸收)

1. **B6 第 4 步的 `go test -race` 在本机跑不了**。
   - 依据:`docs/PROGRESS.md:5915`、`:6002` 记录本机没有 gcc,cgo 起不来;`:4925` 那次是经用户同意、临时起 Linux 容器才跑通的。
   - 问题:原通过标准写死「无 DATA RACE」,等于要求 Codex 去装工具或起容器,触 AGENTS §10.2。
   - 处理:-race 拆成可选的第 4b 步;没有 gcc 就如实报「未跑」,不得为它装工具。B12 第 7 条同步登记。

## 重要(6 条,已吸收)

2. **漏了入场链换池**。
   - 快路径在 PreloadPool 的 worker 里同步调 `onComplete`(`ensure_player_all_data_async.go:65-67`),整段 `applyLoadedPlayerSession` 都占着池 worker。
   - 冷读路径发完 Kafka 就把 worker 还回去,apply 跑在 dispatcher 的 16 个 worker 上(`task_result_dispatcher.go:82`、:288-300)。
   - 开种子后新号也走前者,池占用变长;池是非阻塞提交、Size 256,池满回 `kLoginInProgress`。
   - 处理:补进 §1 代价、§2.2、§6 第 21 行、§7 第 4 步、§9 第 7 条、R8、B2.9 yaml 注释、B6 冒烟、B12 第 6 条。
   - 不改代码:老玩家命中缓存今天就是这条路,只需观测,按需调大 `PreloadPool.Size`。
3. **违规配置只覆盖了一半**。
   - `player-storage-placement.md` §13 第 3 条禁止两件事,原规格只覆盖了「Required 开着而新号没落点」(`missing_required`)。
   - 另一件是 `NewPlayerStorageId` 指向打不开的库。写任务按 `errPlacementStoreUnavailable` 延后,不耗重试预算,一直在重试队列里转(`placement_route.go:36-37`、`key_ordered_consumer.go:2072-2107`)。今天读任务会耗尽重试,新号进不去;开种子后同样变隐蔽。
   - 处理:告警加上 `db_placement_store_open_total{result=~"error|rejected"}`。改动落在 §1、§6 第 16 行、§7 第 3 步、§9 第 2 条、R3、P5、B2.7、B2.9、B2.10、B2.11。
4. **锁 TTL 的预算算术不对了**。
   - 原 `login.yaml:65-72` 的最坏和是 gRPC 3×3s + Redis 3×3s = 18s,所以「20 盖住」。
   - 开种子后 Redis 变 4 次,最坏和 21s,超过 `AccountLockTTL: 20`。
   - 原规格只加一行「还有一次 Redis」,紧接着的「20 盖住」在开关打开时就成了错话。
   - 处理:B2.9 第 1 处改成三行,如实写 21s;§9 第 1 条改写;新增 P6,默认不调 TTL,正确性靠围栏。
   - 顺带核对了 zrpc `Timeout: 10000` 的注释:它按「常态 Redis 毫秒级」估算,种子在常态下也是毫秒级,不用改。这一点写进了 §9 第 1 条。
5. **§6 第 1 行「种子没让情况更糟」不准确**。
   - 场景:库里有旧行,而 Redis 侧的旧父键和 owner_epoch 都没了。
   - 今天:冷读把旧行读给新角色,资产串给新账号,旧行还在。
   - 开种子后:新角色从空档起步,首存覆盖旧行,旧玩家的进度被抹掉。
   - 两者都是事故,但形态不同,会影响 P2 的取舍。
   - 处理:改写第 1 行与 R1。
6. **B4 承诺「种子结果不改变建角结果」,但 error 分支没有用例**。
   - 处理:新增 `TestCreatePlayer_SeedErrorDoesNotFailCreate`,把父键预置成 list,让脚本 WRONGTYPE 报错。
   - B1、B6 的用例数从 7 改为 8;B6 的重点核对补上 miniredis 的这条行为。
7. **同一句症状描述有四处,原规格只改了两处**。
   - 原来改的:`login.yaml:231`、`player-storage-placement.md:448`。
   - 漏掉的:`config.go:163-166`、`deploy/login-stack.linux/login.yaml:150`。
   - 处理:补进 B2.7 第 3 处、B2.10 第 1 处,B1 清单同步。

## 小问题(8 条,已吸收)

8. B2.3 中 parent_exists / owner_epoch_exists 的 ERROR 日志写的是「first EnterGame falls back to the DB read」,与 B4 矛盾:父键在时走的是快路径。已改文案并加注释。
9. B5.2 的 `lockStealingRegisterClient` 新类型没有必要。同包有现成先例:`newNameHarness` 加 `n.fake.reserve` 删锁(`TestCreatePlayerName_LostLockFencesAccountWrite`)。已改用先例,不新增类型。
10. B5.1 的 `RedisDownIsError` 用 `mr.Close()`,耗时依赖 go-redis 拨号重试的退避。已改用 `mr.SetError`(同包先例)。
11. B6 冒烟把 go/db 指标写在 login 的 :9101 下,go/db 实际是 :9160(AGENTS §6.1)。已改;同时补了看 `[preload-pool]` 日志。
12. `PrimeCreatePlayerMetrics` 函数注释的首句也要改。另外,开关关着时也会预置两条 0 序列,这是无害的。已写进 B2.5。
13. 过时的 C++ 注释漏了一处:`player_lifecycle.cpp:2037-2041`「CreatePlayer only creates an account entry」。已补进 §9 第 5 条与 B1 后续批。
14. §6 第 15 行「今天冷读写出的父键也有同样的窗口」不准确。今天的首次进场碰不到这个窗口,因为冷读刚写的父键带满 24h;种子把窗口扩到「建角约 24h 后才首次进场」的新号。已改写;结论不变,仍是残余风险,P4 仍不做。
15. §7 回退补一句:go-zero 配置不热加载,改开关要滚动重启 login。

## 考虑过、未采纳

- **按 `l.ctx` 剩余预算跳过种子**。想法是 Redis 变慢时别让可选步骤吃掉 10s 服务端预算的尾巴。
  - 不采纳的原因:常态下种子是毫秒级;Redis 慢时,账号 blob 的读写本身就同样慢,跳过种子救不回建角。
  - 多一个结局 label 和一组用例,不划算(KISS)。

## 核对后成立、不改的点

- **编译面**:
  - 删掉 `ensure_player_all_data_async.go` 的 `login_proto_database` import 是对的,它只在 :49-51 用到。
  - dataloader 不 import clientplayerlogin,`metrics.go` 引 dataloader 不成环。
  - `shared/ownerepoch.OwnerEpochKey` 存在,只依赖标准库;`shared` 已经 replace 进 login,没有 vendor 和 go.work。
  - 新符号和现有名字没有冲突。
  - `playerAllDataParentTTL` 是 `time.Duration` 常量,可以直接调 `.Milliseconds()`。
  - `Script.Run(…).Int()`、`metric.CounterVec.Add(0, …)` 都有同包先例。
- **Lua 语义**:
  - `GET` 缺键返回 false。
  - owner_epoch 的值是 "0" 时 `EXISTS` 也返回 1,这样更保守。
  - owner_epoch 只经 INCR 前进,从不删除;C++ 守卫脚本在缺键时会补种。
  - C++ 首存是不带 KEEPTTL 的裸 SET,会去掉种子的 TTL。
- **种子内容**:
  - `PlayerAllData` 没有 package,键就是 `PlayerAllData:{id}`。
  - go/db 读空时,`publishDBReadResult` 回写的正是请求体,所以冷读组装结果与种子逐字段相同。
  - C++ 判断首次注册看的是 `registration_timestamp`(`player_lifecycle.cpp:3529`),不看父键在不在。
- **其他读者**:
  - friend 在线目录要求会话在线;好友资料只填显示态。
  - match presence 尽力而为。
  - 资产账本为空时判 UNKNOWN。
  - 回档孤儿报告按映射判断,不看 PlayerAllData。
  - 合服 copy 模式删缓存后冷读读空,结果相同。
  - `dirty_keys_set` 没有 Go 消费者,种子也不加入它。
- **合服窗口**:merge 围栏在 6d 就挡住建角,种子不会在合服期间落地。
- **配置**:
  - K8s 契约测试只核对点名的几个键,加新块不会破坏它。
  - K8s 不写 Placement,同样不写 NewPlayerSeed,整块缺失就是关。
  - `deploy/login-stack.linux/login.yaml` 没有锁预算注释,不用改。
- **测试替身**:
  - `newEnterWiringHarness` 的 svc 不装 KafkaClient 和 TaskResultDispatcher,走到冷读分支会 panic,被 ants 回收。所以 `setSessionCalls==1` 能证明链走的是快路径。
  - miniredis v2.35 支持 KEEPTTL,同包 `TestBackfillPlayerIdentity_WritesClassAndNameTogether` 已经依赖这一点。
- **Java 版**:`CreatePlayerHandler.java:222` 的 `createPlayerWithinCap`、`EnterGameHandler.java:273` 的 `findPlayer`、`StoragePlayerRepository.java:183-195` 都已核对,「不适用」成立。
