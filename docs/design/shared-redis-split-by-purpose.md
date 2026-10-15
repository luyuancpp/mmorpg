# 共享 Redis 按用途拆实例(G4a 设计稿)

> **状态**:设计稿,2026-10-12 起草。**未落码、未编译、未测试、未压测**。本稿只读代码、grep 和 git 得出(AGENTS.md §10.1),
> 行号以 HEAD `73b852f32` 加当前工作区(D1 映射持久化第 0、1a 批的在途改动)为准。
> **起因**:用户 2026-10-12 列出开服首日的五个瓶颈,第 3 条是「共享 Redis 只有一个主节点,所有 zone 共用」,并追问「万一归属全服库成瓶颈了呢」。
> 对应开服准备清单 L-06(`docs/design/tidb-phase1-and-launch-readiness.md:817`)和
> `docs/design/no-single-node-horizontal-scaling-20261001.md` §5(`:234-237`,当时写「方向待定」)。
> **范围**:只按用途拆实例。不改任何键名格式,不改任何 Lua,不上 Redis Cluster,不做哨兵 HA。HA 与集群化是后续项(§13)。

## 1. 结论

1. **拆成「1 个核心 + 2 个新实例 + 1 个可选实例」**:

   | 实例(K8s Service) | 用途 | 批次 |
   |---|---|---|
   | `redis`(原实例,不改名、不重建) | 玩家归属与运行时核心:会话、锁、`PlayerAllData` 父键、归属 epoch / 位置 / 交接标记、场景路由、战斗与组队契约、账号目录 | 现状 |
   | `redis-queue` | 排队与限流:login 排队 / 准入 / gate 排空标记,Java gateway 的 Bucket4j 令牌桶 | 第 1 批 |
   | `redis-mapping` | 映射:归属、落点、合服围栏、能力标记;第 2 批起再并入 go/db 存盘控制面(已落库游标、applied epoch、排序锁、重试收据) | 第 1 / 2 批 |
   | `redis-cache`(可选) | 子表缓存与读结果:`PlayerDatabase*:{id}` 子表缓存、`task:result:*` | 第 3 批 |

2. **任务提出的四类里,有两类不能照字面拆**:
   - 「会话与锁」不能单独成实例。login 的身份补齐脚本在一段 Lua 里同时读写 `PlayerAllData` 父键、玩家登录锁、`player:session`、`player:{id}:location`(§2.3)。
   - 「PlayerAllData 缓存」只有**子表缓存和读结果**能挪走。父键 `PlayerAllData:{id}` 被 C++ 存盘 Lua 与 `player:{id}:owner_epoch` 原子绑定;C++ 每个线程只有一条 Redis 连接,还依赖这条连接的先后顺序(§2.4)。
   - 所以这些键只能一起留在核心实例。
3. **拆完后核心仍是全服单主**。拆分只做两件事:把最吵的流量(排队轮询、限流)挪走;把要求最严的状态(映射、游标:丢一个键就静默写错库)挪到可以单独配 noeviction、WAITAOF、回档守卫和容量告警的实例上。核心的容量和单点问题,下一步只能靠哨兵 HA(只解决可用性)或区域集群(`tidb-phase1-and-launch-readiness.md` §8)。
4. **默认全关**:部署参数 `-RedisSplit` 缺省为空时,拆分相关的渲染结果与今天逐字节相同。本机 docker-compose 继续单实例(`no-single-node-horizontal-scaling-20261001.md:27`)。
   第 1a 批只有两处**有意的缺省即生效**改动,都不属于拆分本身,单独声明:
   - release 档 gateway ConfigMap 补 `spring.data.redis.password`(修 §2.5 第 4 条)。只在注入的口令非空时写,所以 dev 档渲染逐字节不变,release 档会多一行;
   - 回档 FLUSHDB 守卫补核心族(§4.6)。今天核心上已经有映射族,守卫本来就会拒绝,所以现在看不到行为变化;拆分后它防的是「核心上没有映射族了,FLUSHDB 被放行」。
   login 的 Dial / Read / WriteTimeout 没传进 client(§2.5 第 8 条)**不在本稿修**:传进去会让核心 client 的 DialTimeout 从 go-redis 缺省 5s 变成 3s,这是与拆分无关的行为变化,另行登记。
5. **迁移顺序固定为**:停写 → 拷贝 → 校验 → 切换 → 解冻 → 清理旧实例。双写、双读都不采用(§5.1)。回退走同一顺序,但用「先清目标再拷」的 `restore`,不能用只补缺的 `delta`(§5.8)。
6. **所有新实例都设 `noeviction`,并显式写 `maxmemory`**。release 档启用了某个用途、却没给它的 `maxmemory`,部署直接拒绝。核心的 `maxmemory` 由 §13 第 4 条拍板,不给就保持现状(Redis 缺省即 noeviction,只是没有上限)。
7. 关于「归属全服库会不会成瓶颈」,Redis 这一侧的回答见 §9:归属目录本身的读写是 O(1) 小键,先到的是第 2 批并进来的存盘控制面。届时把控制面再拆成第四个实例只需改配置。

## 2. 盘点

### 2.1 谁在连共享 Redis

| 运行时 / 服务 | 句柄(配置键) | 客户端与连接方式 | K8s 现在指向 | 备注 |
|---|---|---|---|---|
| C++ gate / scene / battle | `zoneredis`(`cpp/libs/engine/config/config.cpp:283-306`) | muduo hiredis。每个线程一条连接 `tlsRedis.GetZoneRedis()`(`cpp/libs/engine/thread_context/redis_manager.h:25-28`、`:56`);启动时只解析一次地址(`cpp/libs/engine/core/node/system/node/node.cpp:541-557`) | `redis.<infra>`,password 写空串(`tools/scripts/k8s_deploy.ps1:1866-1874`) | **不发 AUTH,也不发 SELECT**:password、db 读进了配置,但没有任何代码使用(§2.5 第 3 条) |
| go/login | `Node.RedisClient`(`go/login/internal/config/config.go:405-424`) | go-redis v9,单个 client(`go/login/internal/svc/servicecontext.go:107-120`) | `:3583-3590` | 还有 `AuthProviders.SaToken.Redis`(`internal/svc/auth_init.go:67-80`),K8s 不配 |
| go/db | `ServerConfig.RedisClient`(`go/db/internal/config/config.go:69-75`、`:181-186`)+ `Placement.Redis`(`:47-60`,缺省复用前者) | go-redis(`go/db/internal/svc/servicecontext.go:21-55`) | `:3456-3465`,不写 Placement 段 | 落点句柄与排序锁 / 游标 / 缓存句柄「在部署上可以是不同实例」(`go/db/internal/kafka/placement_route.go:53-55`) |
| go/data_service | `MappingRedis`(go-zero,`go/data_service/internal/config/config.go:15`);`DevRedis` / `Regions`(`:16-17`、`:505-514`) | go-zero + go-redis | MappingRedis `:3480-3483`;Regions `:3484-3490`;DevRedis `:3491-3495` | DevRedis 一配就短路 Regions(`internal/routing/router.go:433-450`) |
| go/player_locator | `RedisClient`(`go/player_locator/internal/config/config.go:11`) | go-redis | `:3699-3702` | |
| go/scene_manager | `Redis`(go-zero RedisKeyConf) | go-zero | `:3731-3735` | |
| go/match | `Redis`(共享)+ `MatchRedis`(`go/match/internal/config/config.go:21`,match 自己的 Redis Cluster) | go-zero / go-redis | `:3792-3800` | |
| go/chat | `Redis`(共享;v1 不读任何契约键,`go/chat/internal/svc/servicecontext.go:29-33`)+ `ChatRedis`(`internal/config/config.go:36`) | go-zero | `:3869-3882` | ChatRedis 复用 match 的集群 |
| go/trade | `SharedRedis`(`go/trade/internal/config/config.go:95-109`) | go-redis | `:4008-4011` | 只读 `player:{id}:location` |
| go/friend | `Redis`(共享)+ `FriendRedis`(`go/friend/internal/config/config.go:93-95`) | go-zero | `:4056-4064`;FriendRedis 刻意不配,回落共享库 | |
| go/guild | `RedisClient`(DB 2)、`PlayerLocatorRedis`(DB 0)、`MergeMarkerRedis`(`go/guild/internal/config/config.go:41-42`、`:60`;`go/guild/etc/guild.yaml:14-38`) | | **K8s 上没有清单**(no-single-node §7) | |
| Java gateway | `spring.data.redis.*`(`java/gateway_node/src/main/resources/application.yaml:20-23`);Bucket4j 用专门建的 Lettuce 连接(`ratelimit/RateLimitConfig.java:40-57`) | Lettuce | `:4935-4938`,**没有 password**。**按 zone 部署**:ConfigMap、Deployment、Ingress 都在 zone namespace,随 zone-up 走(`k8s_deploy.ps1:217`、`Apply-JavaSvcManifests`),不是全局服务 | `ZoneHealthProbeService` 注入了 `StringRedisTemplate`,但没用到(`service/ZoneHealthProbeService.java:31`) |
| Java auth(sa-token-auth) | `spring.data.redis.host`(`k8s_deploy.ps1:4914-4916`) | Spring Data Redis | `redis.<infra>`,没有 password | sa-token 会话。本稿不动它,留在核心;契约测试一并钉住 |
| 运维工具 | merge_zone 有五组句柄(`tools/merge_zone/main.go:252-281`);`k8s_gate_drain.ps1` 读 login 的 `Node.RedisClient`(`:312-330`、`:368`);`k8s_zone_rollback.ps1 -RedisHost`(`:844-853`);data_consistency_check(`main.go:101-103`);`route_tool` 读 data_service yaml;go/db `cmd/verifier`、`cmd/data_stress`;robot `data_stress.go:71`;data_service `cmd/debug_fetch` | | | |

### 2.2 键族清单

「写 / 读」一栏里,C++ 指 gate / scene / battle。频率的量化放在 §6.1,这里只给定性说明。

**A. 排队与准入(login)**

| 键 | 写 | 读 | 频率 | TTL | 多键原子 | 证据 |
|---|---|---|---|---|---|---|
| `queue:zone:{z}` ZSET、`queue:meta:{qid}` HASH、`queue:token:{token}` | login Enqueue | login Lookup(每次轮询 ≥4–5 条命令) | 排队开启时最高频 | `QueueEntryTTL` 1h | Enqueue 用 MULTI 写三键 | `go/login/internal/logic/pkg/loginqueue/queue.go:138-142`、`:170-183`、`:226-284` |
| `admit:{qid}`、`admitted:zset:zone:{z}` ZSET(成员逐个到期;2026-10-13 G4b-G 起,此前是 `admitted:zone:{z}` SET) | login 放行循环、快速通道 | login | 中 | `admit:{qid}` 为 `AdmitTTL` 60s;占位成员按分数到期,整键 TTL `AdmitTTL + 1min` 只做垃圾回收 | 放行 MULTI(`:465-469`);计数与快速通道是单键 Lua(`:495-498`、`:521-527`);取走后续期是单键 MULTI(`:387-389`) | 同上 |
| `dispatcher:lock:login_queue` | login 选主 | login | 1/s | 30s | 单键 | `go/login/internal/config/config.go:244-258`;`go/login/etc/login.yaml:436`。键是全局的,L-01 缺陷相关 |
| `gate:{id}:draining` / `gate:{id}:drained` | `k8s_gate_drain.ps1` 的多键 Lua;login 排空循环 | login AssignGate 一次 MGET(失败时放行) | 低 | 有 | MarkScript 同时碰本台的 draining、drained 和同 zone 其它候选的 draining | `tools/scripts/k8s_gate_drain.ps1:150-158`;`loginqueue/gatedrain.go:30`、`:47-81`;`gatedrain_monitor.go:37`、`:129-181` |

排队默认关闭:`login.yaml:428-429`;K8s 的 login ConfigMap 不写 Queue 段,取缺省值 false。

**B. 开服限流(Java gateway)**

| 键 | 写 / 读 | TTL | 证据 |
|---|---|---|---|
| `rl:zone:{z}`、`rl:ip:{ip}` | Bucket4j 做 CAS,每个请求碰 2 个桶 | 桶重填满的时间 + 1h | `AssignGateRateLimiter.java:150-154`;`RateLimitConfig.java:71-91`。默认关(`application.yaml:36-38`) |

**C. 映射(data_service `MappingRedis`)**

| 键 | 写 | 读 | TTL | 多键原子 | 证据 |
|---|---|---|---|---|---|
| `player:zone:{id}` | data_service W1(建角时一次);merge_zone | data_service 单查 / 批查、go/db MGET、merge_zone、data_consistency_check | 无 | W1 的 Lua 里复核 `merge:in_progress`;「读 home 带围栏」那段 Lua 在脚本内拼出围栏键 | `go/data_service/internal/routing/router.go:86`、`:152-237`、`:288-324`、`:366-390` |
| `player:placement:{id}` | merge_zone 钉落点 / 搬库 CAS | go/db 每个任务 MGET 两次(选库一次,复核一次) | 无 | 映射族内部 | `go/shared/placement/placement.go:30-41`;`go/db/internal/kafka/placement_route.go:90`、`:242-254` |
| `player:maprev:{id}`、`mapping:route:ready` | D1 第 1a 批只定义了这两个键 | — | 无 | 发布脚本碰 zone / placement / maprev / 围栏四键 | `go/shared/placement/route.go:21`、`:30` |
| `merge:in_progress:{z}`、`merge:merged_into:{src}` | merge_zone | data_service、guild | 前者有 TTL(D1 计划改成无 TTL),后者永久 | 映射族内部 | `router.go:110`;`tools/merge_zone/fence.go:53`;`merged_into.go:35`;`go/guild/internal/logic/merge_fence.go:62-73` |
| `db:capability:zone:{z}` | go/db 每 30s 续写 | merge_zone | 90s | 单键 | `placement.go:106-112`;`placement_route.go:384-386`、`:404`;`go/db/db.go:143-157` |
| `lock:player:{id}` | data_service | data_service | `PlayerLockTTLSec` = 3s | 与 DevRedis 上的数据锁组成**跨实例双锁**(设计如此,本来就不是原子操作) | `router.go:525-533`、`:545`;`k8s_deploy.ps1:3496` |
| `player:name:{id}` | data_service 名字缓存(含负缓存) | data_service | 缓存 | 单键 | `go/data_service/internal/routing/player_name_cache.go:28-29` |

**D. go/db 存盘控制面(go/db `ServerConfig.RedisClient`)**

| 键 | 读写 | TTL | 多键原子 | 证据 |
|---|---|---|---|---|
| `consumer:applied:{topic}:{id}:{msgType}` | 每个写任务读一次、写一次 | **无** | 单键 | `go/db/internal/kafka/key_ordered_consumer.go:420`、`:482`、`:518` |
| `consumer:applied_epoch:{topic}:{id}:{msgType}` | 每个写任务读一次,单键 Lua 推进 | **无** | 单键 | `:547-596` |
| `distributed:lock:kafka:ordering:{topic}:{id}:{msgType}` | 每个写任务 SETNX / 续期 / 释放 | 有(租约) | 单键 | `go/db/internal/locker/locker.go:29`;`key_ordered_consumer.go:1852` |
| `kafka:retry:queue\|processing:{topic}[:{inst}]`、`kafka:retry:instances:{topic}`、`kafka:dead:queue:{topic}` | 重试路径 | **无** | 多键 Lua,只在本族内 | `key_ordered_consumer.go:903-908`、`:1079`、`:1089-1104`;`retry_ownership.go:63-66`、`:107-146` |
| `distributed:lock:kafka:consumer:lock:{playerID}`、`kafka:expand:status:{topic}` | 锁只在在线扩分区期间(或读扩分区状态出错时)用;状态每个任务读一次 | 有(5s)/ 无 | 单键 | `key_ordered_consumer.go:1820-1832`、`:1959`;`go/shared/kafkautil/expand_utils.go:104-105`。**锁键经 locker 加了 `distributed:lock:` 前缀(`locker.go:29`),而且只含 player_id、不含 topic**:它是全服共享的一把键,不能按 zone 过滤(§3.2.3、§5.2) |

只读方:merge_zone 的 preflight 与审计检查重试 / 死信队列(`tools/merge_zone/preflight.go:164-199`、`:260-270`;`audit_checks.go:153-165`);relocate 的 R2 步骤等排序锁释放(`relocate.go:367-372`)。

**E. 子表缓存与读结果**

| 键 | 写 | 读 | TTL | 证据 |
|---|---|---|---|---|
| `PlayerDatabase:{id}`、`PlayerDatabase_1:{id}`(键名 = proto 全名) | go/db:写任务落库后回写;读任务回写 | login 进场(父键缺失时)、data_service `debug_fetch` | `DefaultTTLSeconds`:K8s 3600s(`k8s_deploy.ps1:3458`),缺省 24h | `key_ordered_consumer.go:54-64`、`:369`、`:846-857`;`go/login/internal/logic/pkg/dataloader/ensure_player_all_data_async.go:83-84` |
| `task:result:{taskID}` LIST + 频道 `task:result:notify` | go/db 写 LIST,再 PUBLISH | login 结果分发器 | 5min | `key_ordered_consumer.go:37`、`:42`、`:393-411`;`go/login/internal/dispatcher/task_result_dispatcher.go:32`、`:324`、`:370`。**单频道,PUBLISH 会扇出到所有 login 实例** |

merge_zone 合服与撤销时会按玩家删缓存(`tools/merge_zone/player_rows.go:718-745`;`merge_run.go:455`;`unmerge.go:259`)。

**F. 核心:玩家归属与运行时(全部留在 `redis`)**

| 键 | 写 | 读 | TTL | 证据 |
|---|---|---|---|---|
| `PlayerAllData:{id}` 父键 | C++ 存盘 Lua(写入后**无 TTL**);login 组装父键(24h);login 身份补齐 Lua(KEEPTTL) | C++ 载入;login;data_service 资产账本;match 组队在线判定 | 见左 | `cpp/libs/engine/infra/storage/redis_client/redis_client.h:42-52`、`:212`、`:491`;`dataloader/data_loader.go:44-45`;`sync_loader.go:125`;`go/data_service/internal/logic/asset_op_ledger_logic.go:25`、`:79-89`;`go/match/internal/team/presence.go:50-52` |
| `dirty_keys_set` | C++ 每次存盘 SADD | **没有读者** | 无 | `redis_client.h:22`、`:50` |
| `player:{id}:owner_epoch` / `:handoff` / `:location` | scene_manager(铸造、CAS、回滚);C++(交接标记、取证) | C++、scene_manager、player_locator、match、trade、login(Lua)、battle 节点、merge_zone | owner_epoch 永不删;handoff 300s | `go/shared/ownerepoch/ownerepoch.go:14-56`;`cpp/libs/services/scene/player/comp/player_ownership_comp.h:54-66`;`go/shared/scenenode/locator.go:18-36`(列了这个格式的 7 处实现) |
| `player:session:{id}`、`player:location:{id}`(旧)、`player:leases`、`player:leases:processing`、`player:leases:claim_tokens`、`player:leases:claim_payloads` | player_locator 的 9 段 Lua | friend、guild、match、merge_zone、login(Lua) | 断线后 session 带 PX | `go/player_locator/internal/logic/keys.go:5-27`;`session_cas.go:29-215` |
| `account:{account}` | login,写时带建角锁做围栏 | login | **无;权威数据只有这一份,没有库可以回源** | `go/login/internal/logic/clientplayerlogin/loginlogic.go:318-345`;`createplayerlogic.go:62-73` |
| `player_to_account:{id}` | login 建角 | 回档时的孤儿清理 | 无 | `createplayerlogic.go:304-305`;`go/login/internal/constants/login_constants.go:18-22` |
| `login_account_sessions:{account}`、`login_session:{sid}`、`login_idempotent:*`、`access_token:*`、`refresh_token:*`、`account_refresh:*` | login | login | 有 | `login_constants.go:5-16`;`pkg/loginsession/loginsession.go:38-49`;`pkg/sessionmanager/session_manager.go:47-48`;`pkg/token/token.go:31-33`、`:105-112`(令牌三键走 MULTI) |
| `account_lock:create\|login:{account}`、`player_locker:{id}` | login | login | 有;玩家锁 120s(`login.yaml:88`) | `createplayerlogic.go:118`;`loginlogic.go:55`;`entergamelogic.go:103` |
| `player:{id}:login_generation`(2026-10-14 新增) | login 登录链持锁铸造(一段 Lua) | **只有铸造脚本自己读**;校验方比的是会话记录里的值 | 7 天;**可丢**:重建以毫秒时间与会话记录代次为下限,不回退,不必进清库 / 回档 / 合服清单 | `go/login/internal/logic/pkg/sessionmanager/login_generation.go`;设计见 [scene-enter-session-fence.md](scene-enter-session-fence.md) §3 |
| `player_merge_notice:{id}`、`player_force_rename:{id}` | merge_zone | login | — | `tools/merge_zone/post_merge_stamp.go:65-66`;`entergamelogic.go:494-531` |
| 场景路由:`scene:{id}:node\|zone\|mirror\|source\|mirrors\|draining\|agones_gs`、`instances:zone:{z}:active`、`instance:{id}:player_count`、`node:zone:{z}:{node}:scenes\|death_at`、`scene_nodes:zone:{z}:load`、`world_channels:*`、`world_channel_directory:zone:*`、`world_channel_lineno:*`、`world_init:lock:zone:{z}`、`enter_scene:dedup:{id}:{req}`、`player:{id}:channel_switch_cooldown`、`scene_manager:leader:lock` | scene_manager | scene_manager;C++ 读频道目录(`cpp/libs/services/scene/player/system/player_scene.cpp:39`);merge_zone | 部分有 | `go/scene_manager/internal/logic/createscenelogic.go:19-40`;`load_reporter.go:23`;`reentry_barrier.go:39`;`channel_pick.go:50`;`agones_binding.go:25`;`enterscenelogic.go:204`;`world_*.go`;`go/scene_manager/scene_manager_service.go:94-96` |
| 战斗:`battle:lock:{id}`、`battle:ctx:{id}`、`battle:settlement:pending[:id]:{id}`、`battle:activity_result:*` | C++ scene;battle 节点 | match、C++ | 有;**2026-10-15 起 `battle:settlement:pending[:id]:{id}` 不再过期**(去掉 7 天 TTL,保留到应用为止,每玩家单槽) | `cpp/libs/services/scene/battle/system/player_battle.cpp:94-95`;`cpp/libs/services/battle/settlement/settlement_outbox.h:98-111`;`battle/system/battle_result_activity.h:30`;保留期改动见 [landing-recovery-sync-digest.md](landing-recovery-sync-digest.md) §7 |
| `battle:settlement:pending:index`(2026-10-15 新增,全局一把 ZSET) | battle 节点、C++ scene(与写入 / 销账同一段 Lua 内 ZADD / ZREM) | battle 节点报数(积压数、最老年龄)与孤儿清理 | 无;**可丢**:只用于监控,丢了只影响报数,不影响结算 | `cpp/libs/services/battle/settlement/settlement_outbox.h`;设计见 [landing-recovery-sync-digest.md](landing-recovery-sync-digest.md) §7 |
| `player:{id}:sync_digest`(2026-10-15 新增,hash) | friend / match(team)/ guild 在业务提交后 bump(一段单键 Lua) | C++ scene(`HGETALL`,`player_landing.cpp`) | 30 天,每次 bump 续期;**可丢**:丢失或过期即代次 `g` 改变,客户端全量拉一次;不必进清库 / 回档 / 合服清单,但回档 Go 域权威数据(friend / guild 库)的工具应删除受影响玩家的这把键 | `go/shared/playersync/digest.go`;`cpp/libs/services/scene/player/system/player_landing.h`;设计见 [landing-recovery-sync-digest.md](landing-recovery-sync-digest.md) §3 |
| 资产唤醒:频道 `assetop:wake`(Pub/Sub,不落键)、去重键 `assetop:wake:{service}:{id}`(2026-10-15 新增) | C++ scene `PUBLISH`;guild / trade 订阅后 `SET NX EX 5` | guild / trade | 去重键 5 秒;**可丢** | `cpp/libs/services/scene/player/system/player_landing.cpp`;`go/shared/assetopwake/`;设计见 [landing-recovery-sync-digest.md](landing-recovery-sync-digest.md) §5 |
| 组队:`team:rec:{tid}`、`team:{tid}`、`team:player:{pid}`、`team:invite:{pid}` | match 的 team 模块(S_COMMIT 一段 Lua 写四类) | C++(`player_team.cpp:43-45`、`:202`、`:291`、`:355`) | — | `go/match/internal/team/keys.go:9`、`:38-53` |
| data_service 玩家数据 `player:{%d}:*`(含 `__lock`、`__version`) | data_service | data_service | — | `go/data_service/internal/logic/data_logic.go:20-25`;`router.go:531-533`。Load / Save 这两个 RPC 在仓库里 grep 不到生产调用方 |

**G. 本稿不处理**:guild 私有键(DB 2,K8s 上未部署:`guild:v2:*`、`guild_rank:*`,见 `tools/merge_zone/guild_step.go:123-128`、`fence.go:239`);friend 私有键(FriendRedis 未配置时回落到共享库);match / chat 的集群键。

### 2.3 跨族原子性:哪些 Lua / MULTI 把几类键绑在一起

| 脚本 | 位置 | 碰到的族 | 对拆分的含义 |
|---|---|---|---|
| C++ `kSaveIfGuardLuaScript` | `redis_client.h:42-52` | 父键 + `owner_epoch` + `dirty_keys_set` | 父键必须与归属 epoch 同实例 |
| C++ `kSaveAndMarkLuaScript` | `redis_client.h:20-24` | 父键 + `dirty_keys_set` | 同上 |
| scene_manager 落点 CAS | `go/scene_manager/internal/logic/changesceneutil.go:153-155` | `owner_epoch` + `location` + `handoff` | 位置与 epoch 同实例 |
| scene_manager 路由失败回滚 | `owner_epoch.go:480-482` | `location` + `owner_epoch` + `handoff` | 同上 |
| scene_manager 离场 | `leavescenelogic.go:65-70` | `location` + 场景人数 + 节点人数 | 位置与场景路由同实例 |
| scene_manager 场景生命周期(多段) | `scene_atomic.go:179-303` | `scene:*` + `instances:*` + `node:*`(最多 3N 键) | 场景路由族内部 |
| C++ 交接标记 | `cpp/libs/services/scene/player/system/exit_release_mark.h:173-182`、`:253-291` | `owner_epoch` + `handoff` | |
| C++ 改派核实 | `relocate_confirm.h:134-148` | `owner_epoch` + `location` + `handoff` | |
| C++ 换图结果取证 | `player_lifecycle.h:717-742` | `handoff` + `owner_epoch` + `location` | |
| **login 身份补齐** | `go/login/internal/logic/clientplayerlogin/player_class_backfill.go:91-95`、`:179-191` | **父键 + `player_locker` + `player:session` + `location`** | **会话、锁、父键、位置必须同实例** |
| login 外观修复 | `player_class_backfill.go:157-158`、`:168-174` | `account` + `player_locker` + `account_lock:create` | 账号目录与锁同实例 |
| login 写账号 blob | `createplayerlogic.go:62-73`、`:624` | `account` + `account_lock:create` | 同上 |
| player_locator 会话生命周期(9 段) | `go/player_locator/internal/logic/session_cas.go:29-215` | `player:session` + `player:location` + `player:leases*` | 会话族内部 |
| C++ 战斗锁与结算 | `player_battle.cpp:170-226`;`settlement_outbox.h:101-111` | `battle:lock` + `battle:ctx` + 结算待销账记录 | 战斗族内部 |
| match 组队 S_COMMIT 等 | `go/match/internal/team/keys.go:9`;`scripts.go` | `team:*` 四类 | 组队族内部(C++ 读) |
| login 排队 | `queue.go:170-183`(MULTI)、`:428-431`(MULTI)、`:456-463` | 排队 / 准入 | 排队族内部 |
| gate 排空标记与撤回 | `k8s_gate_drain.ps1:150-158` | `gate:{id}:draining` + `drained` + 同 zone 其它台的 draining | 排队族内部 |
| login 签令牌 | `token.go:105-112`(MULTI) | `access_token` + `refresh_token` + `account_refresh` | 会话族内部 |
| data_service W1 注册 | `router.go:152-237` | `player:zone` + `merge:in_progress` | 映射族内部 |
| data_service 读 home 带围栏 | `router.go:299-324` | `player:zone` + 脚本内拼出的围栏键 | 映射族内部(脚本内拼键,单实例才成立) |
| D1 发布脚本(db_first 模式) | `go/shared/placement/route.go` 的 `PublishRouteScript` | zone / placement / maprev / 围栏 | 映射族内部 |
| merge_zone 改映射、relocate CAS、merged_into | `tools/merge_zone/merge_run.go:839-951`;`relocate.go`;`merged_into.go:93-119` | 映射族内部 | |
| go/db 重试收据 | `retry_ownership.go:107-146`;`key_ordered_consumer.go:1089-1104` | `kafka:retry*` | 控制面内部 |
| data_service 删除时的双锁 | `router.go:545` 起(`AcquirePlayerLock`) | `lock:player`(映射)+ `player:{id}:__lock`(DevRedis) | **本来就按跨实例设计**,不依赖原子性 |

**结论**:
- 父键、`owner_epoch`、`handoff`、`location`、`player:session`、`player_locker`、`account`、`account_lock`、场景路由这一组键,通过上表的脚本两两相连,构成一个闭包。它们只能整体留在同一个实例。
- 排队、映射、控制面、子表缓存与读结果这几族,多键操作都只在自己族内。它们各自可以独立成实例。

### 2.4 Lua 之外的跨族约束

1. **C++ 依赖单连接的先后顺序**:
   - 存盘客户端用的就是 zone 连接本身(`cpp/libs/services/scene/core/system/redis.cpp:19`)。
   - 结算时「存盘后紧跟一条 PING,PING 回来就判定已落盘」(`player_battle.cpp:385-393`)。
   - 「两条命令必须走同一条不重放的连接」(`player_lifecycle.h:734-735`)。
   - 所以 C++ 的键即使不在同一段 Lua 里,也依赖「同一实例、同一连接按顺序执行」。
   - 只要动 C++ 的键,就得先给 `RedisManager` 加第二条连接,再逐条重新论证顺序。**本稿不改 C++**。
2. **go/db 持锁选库与搬库 R2**:
   - 「选库结果必须在持锁期间用掉 —— 锁挡着搬库工具的等在途写完成(R2)」(`placement_route.go:258-261`)。
   - 锁属于控制面,落点记录属于映射,两者今天已经分属两个句柄。
   - 拆到两个实例之后,「go/db 先拿锁再读落点」与「工具先写冻结再查锁」仍然成立:每个实例各自线性一致,先后由单个进程的执行顺序保证。
   - 所以第 1 批只挪映射、把控制面暂时留在核心,正确性不受影响。
3. **D1 §5.2 的论证**(`docs/design/player-route-mapping-durability-proposal.md:239`):原文是「Redis 整体丢失时,游标、排序锁、重试队列也在同一实例一起丢,回源换不来可用性」。
   - 第 2 批把控制面并进 `redis-mapping` 之后,这条论证原样成立。
   - 第 1 批和第 2 批之间暂时不成立,见 §11 第 3 条。
4. **既有契约**:
   - CZ-2:`PlayerAllData` 所在的 Redis 物理共享(`docs/design/cross-zone-scene-travel.md:31`)。
   - D12:SharedRedis 必须是全服单一实例(`docs/design/cross-zone-matchmaking.md:103`)。
   - 核心实例保持全服一份,所以这两条契约都不受影响。映射、排队、控制面不在这两条契约列举的键里。
5. **data_service 的发现键不分 zone**:login 经 etcd 键 `dataservice.rpc` 找 data_service(`k8s_deploy.ps1:3469-3474`;`go/login/etc/login.yaml:166-170`)。
   - 也就是说,任何 zone 的 login 都可能调到任何 zone 的 data_service。
   - 所以切换映射地址时,**所有 zone 的 data_service 必须同一拍切**。

### 2.5 顺带发现的既有问题(本稿不修,只登记)

1. **`dirty_keys_set` 只写不读**。C++ 每次存盘都 SADD,全仓 grep 找不到任何读、SREM 或 DEL(只有 `redis_client.h:22`、`:50` 和 `cpp/tests/currency_test`)。结果是一个随「累计存过盘的玩家数」无限增长的大 SET。
2. **核心内存无上界**:
   - `PlayerAllData:{id}` 一经 C++ 存盘就没有 TTL,全仓也 grep 不到删除点。
   - `owner_epoch` 按设计永不删除(`redis_client.h:31-37` 的注释)。
   - 所以核心的内存随「累计进过场景的玩家数」线性增长。
   - redis-oom 问答记录里规划的「冷玩家主动卸载」没有实现(`docs/notes/redis-oom-data-safety-qa-record.md:314-316`)。
3. **C++ 不发 AUTH / SELECT**:
   - 现状:`node.cpp:541-557` 建连时不发;`config.cpp:295-301` 读了 password / db,但没有任何代码使用;K8s 写的是空串(`k8s_deploy.ps1:1869`)。
   - 冲突:release 档注入 ≥12 位密码(`:452-453`),`redis.yaml:93-97` 随之开 requirepass。这时 C++ 节点的所有命令都会 NOAUTH。
   - `docs/PROGRESS.md:5440` 的 release 实测清单也没有列 C++ 和 Java。
4. **Java gateway 的 K8s ConfigMap 没写 `spring.data.redis.password`**(`k8s_deploy.ps1:4935-4938`)。release 档一开限流就 NOAUTH,限流按设计 fail-open。
5. 9 处代码注释仍写「共享库是 allkeys-lfu」(D1 §11.2 已登记)。
6. `redis-ha.yaml:3` 提到的 `-RedisReplicas` 参数在部署脚本里不存在(`tidb-phase1-and-launch-readiness.md:727`)。
7. merge_zone 的 `-notice-redis-*` 一个句柄混了四类:合服提示、`player:session`、go/db 重试队列、go/db 排序锁(`tools/merge_zone/preflight.go:211`;`relocate_run.go:339`)。
8. K8s 给 login `Node.RedisClient` 写了 Dial / Read / WriteTimeout(`k8s_deploy.ps1:3587-3590`),但构造 client 时没传(`go/login/internal/svc/servicecontext.go:111-116`),实际用的是 go-redis 缺省值。

## 3. 拆分方案

### 3.1 目标拓扑

| 实例 | 键族(§2.2) | 客户端 | 持久化 | 淘汰策略 |
|---|---|---|---|---|
| `redis` | F 全部;第 3 批之前还有 E | C++ 全部;login、player_locator、scene_manager、match、trade、friend、chat(回落);data_service 的 DevRedis | AOF everysec(现状) | **noeviction(显式写)** |
| `redis-queue` | A + B | login 新句柄 `Node.QueueRedis`、Java gateway、`k8s_gate_drain.ps1` | AOF everysec | noeviction |
| `redis-mapping` | C(第 1 批)+ D(第 2 批) | data_service MappingRedis;go/db 的 `Placement.Redis`(第 1 批)/ `RedisClient`(第 2 批);guild MergeMarkerRedis;merge_zone;data_consistency_check;route_tool | AOF everysec + WAITAOF(D1) | **noeviction(D1 §3.3 硬前提)** |
| `redis-cache`(可选) | E | go/db 新句柄 `CacheRedis`;login 新句柄 `Node.CacheRedis` | AOF everysec | noeviction(理由见 §6.3) |

### 3.2 逐类说明与跨类约束

#### 3.2.1 排队与限流 → `redis-queue`(第 1 批)

- **能拆的理由**:A、B 两族的多键原子操作都在族内(§2.3);没有 C++ 读写者;键都带 TTL。
- **值得拆的理由**:
  - 排队一开,轮询就是开服首日最吵的流量:每次 Lookup 至少 4–5 条命令(`queue.go:226-284`)。
  - 限流对每个 HTTP 请求做 2 个桶的 CAS。
  - 拆走之后,这股峰值不会再挤压核心上的锁续期。2026-05-25 的 45k 浪涌压测里,Redis QPS 飙升导致丢锁 6.5 万次(`login.yaml:89-93`)。
- **故障隔离**:`redis-queue` 挂掉时:
  - 排空判定放行(`gatedrain.go:60-81`,原有设计);
  - Bucket4j fail-open(`application.yaml:120`);
  - **排队关闭时**:AssignGate 走快速通道,不碰 `redis-queue` 上的排队键,新登录不受影响;
  - **排队打开时**:`FreeSlots` 读 SCARD、快速通道占位、入队都在 `redis-queue` 上,任何一步出错都返回 `StatusError`;占位失败按设计不放行快速通道(fail-closed,`assigngatelogic.go:98-135`)。结果是**新登录全部失败**,直到实例恢复;
  - **已在游戏里的玩家、断线重连的会话判定(读核心)和存盘都不受影响。** 拆分换来的是「排队实例挂了只停新登录」,不是「排队实例挂了还能登录」。
- **约束**:
  1. 同一个 zone 的 login 与 `k8s_gate_drain.ps1` 必须看同一个实例:MarkScript 会同时检查本台和同 zone 其它台的 draining(`k8s_gate_drain.ps1:150-158`)。
  2. **切换期间所有 zone 的排队必须关着**。zone-up 是逐 zone 执行的,做不到「同一拍」;放行循环的锁是全局键(`config.go:253`),排队开着时,切换窗口里两个实例上会各有一个 leader、各有一套队列,准入翻倍。所以部署闸要读线上 login ConfigMap 的 `Queue.Enabled`,为 true 就拒绝切换(§4.4)。
  3. 从第一个 zone 的 login 开始滚动,到最后一个 zone 滚完为止,不许执行 `k8s_gate_drain.ps1`,也不许滚动 gate:脚本按新 ConfigMap 往 `redis-queue` 写 draining,还没滚到的旧 login 读核心,看不到标记,会继续往正在排空的 gate 分人。
- **与 L-01 / L-02 的关系**:
  - L-01(排队 leader 锁)的修复与本拆分无关。
  - **拆分应排在 L-02(打开排队与限流)之前**。今天排队和限流都关着(`login.yaml:429`;`application.yaml:38`),切换时没有在途数据要搬。
- **不并入**:
  - friend 私有键:`FriendRedis` 要求 `Type: cluster`(`go/friend/internal/config/config.go:93-95`)。
  - chat 私有键:已经在 match 的集群里。

#### 3.2.2 映射 → `redis-mapping`(第 1 批)

- **能拆的理由**:
  - C 族的所有 Lua 都在族内;C++ 不读写映射键。
  - go/db 已经有独立句柄 `Placement.Redis`(`go/db/internal/config/config.go:47-60`),data_service 已经有独立的 `MappingRedis`。
  - 所以第 1 批对 go/db、data_service、guild 三方都是**纯配置改动**。
- **值得拆的理由**:
  - D1 §12 已把「mapping Redis 独立成实例」列为开服前事项(`player-route-mapping-durability-proposal.md:775-781`)。
  - 映射是唯一要求「缺键 = 权威缺席」的族。它对 noeviction、AOF、WAITAOF、回档守卫的要求,和核心上的缓存类数据不同。
- **同址契约**:go/db 读落点记录的 Redis 必须与 data_service 的 `MappingRedis` 同址,且是 DB 0(`tools/scripts/tests/k8s_deploy_contract.tests.ps1:839-860`;`k8s_deploy.ps1:3460-3465`)。
  - 第 1 批在 go/db ConfigMap 里显式写 `Placement.Redis.Hosts = redis-mapping`。
  - 契约测试里「显式写了 Placement.Redis 就以它为准」的分支(`:850-853`)原样成立。
- **全局性**:映射全服只有一份,所有 zone 必须同一拍切(§2.4 第 5 条)。
- **随之迁走的键**:`lock:player:{id}`、`player:name:{id}` 走的也是 `MappingRedis` 句柄,一起迁过去。名字缓存可以丢,迁移时不拷。
- **与 D1 的衔接**:
  - 就绪标记 `mapping:route:ready` 记着 `redis_run_id` 和 `master_replid`(proposal `:142`)。换实例等于身份变化,D1 第 1b 批的对账 leader 会撤标记、修补、再挂回(proposal `:262-270`)。
  - leader 挂标记前会自检 noeviction(proposal `:145`)。
  - RouteStore 的 WAITAOF 按 `redis-mapping` 的拓扑取值:单实例用 `NumLocal=1, NumReplicas=0`,也就是配置缺省值。

#### 3.2.3 go/db 存盘控制面 → 并入 `redis-mapping`(第 2 批)

- **为什么并进映射实例,而不是留在核心或单独开实例**:
  1. 两者都是「无 TTL、丢一个键就静默出错」的状态。游标被淘汰,顺序守卫就静默失效(`deploy/docker-compose.yml:228-231` 的注释)。
  2. D1 §5.2「一起丢」的论证恢复成立。
  3. 回档守卫本来就把这两族放在一起检查(`tools/scripts/k8s_zone_rollback.ps1:633-642`)。relocate 的 R1(写冻结,属映射)和 R2(等排序锁,属控制面)也能用一个句柄完成。
  4. 少开一个实例。
- **代价**:
  - 负载:`redis-mapping` 从「每个任务 2 次 MGET」变成「每个任务约 9 条命令」(§6.1)。
  - 内存:除了 O(累计玩家数 × 映射),还要加上 O(累计存过盘的玩家数 × 表数 × 2 个游标)。
  - 如果压测显示扛不住:第 2 批的代码已经让 go/db 的三个句柄互相独立,把控制面拆到第四个实例只需改配置(§13 第 2 条)。
- **为什么要写代码**:go/db 的 `RedisClient` 同时承担子表缓存回写和读结果发布(`key_ordered_consumer.go:353-411`、`:846-857`),这两样必须和 login 在同一个实例。
  - 所以要先加 `CacheRedis` 句柄,缺省等于 `RedisClient`。
  - 有了它,才能把 `RedisClient` 指到 `redis-mapping`,同时让缓存留在核心。
- **可以逐 zone 切,但有一个例外**:游标、epoch、排序锁、重试收据、扩分区状态都带 topic,也就是带 zone(`:420`、`:547`、`:903-908`、`:1852`),只有本 zone 的 go/db 会碰。
  - 例外是在线扩分区用的 `distributed:lock:kafka:consumer:lock:{playerID}`(`:1959`):它只含 player_id,是全服共享的一把键。逐 zone 切换期间,已切和未切的 zone 会在两个实例上各拿各的锁。
  - 所以**第 2 批从第一个 zone 开始、到最后一个 zone 切完为止,全服禁止在线扩分区**。只有扩分区中(`:1835-1838`)或读扩分区状态出错时(`:1821-1823`)才走加锁路径,平时不存在这把锁,所以禁扩分区就够了,不需要改键。读状态出错的那条路径,锁也落在同一个出错的实例上,大概率同样失败、任务延后,不构成跨实例并发。
  - 满足这一条后,可以逐个 zone 停 go/db、拷贝、切换(§5.5)。
  - `db:capability:zone:{z}` 本来就走 `Placement.Redis`,第 1 批已经在 `redis-mapping` 上了。

#### 3.2.4 子表缓存与读结果 → `redis-cache`(第 3 批,可选)

- **能挪的范围**:只有子表缓存和 `task:result`。父键被 §2.3 的两段 Lua 绑在核心上。
- **什么时候做**:压测(L-14)显示核心的内存或 CPU 主要花在下面两处时再做:
  - 子表缓存回写:每个写任务一次大 value 的 SET;
  - `task:result` 的 PUBLISH 扇出:单频道发给全部 login(`task_result_dispatcher.go:32`)。
  它与「`task:result` 按 login 实例分频道」(L-06 后半句)互相独立,**建议先做分频道**。
- **约束**:go/db 与 login 必须**全服同一拍**切。玩家可以在 home 以外的 zone 进场,任何 zone 的 login 都可能在等任何 home zone 的 go/db 发布结果(CZ-2)。
- **为什么可以不拷**:目标实例是空的,缺键就回源读库,结果一定是对的。迁移时不拷,切换后父键缺失的玩家首次进场多一次冷读。
- **反过来,目标上有旧缓存就不安全**:login 父键缺失时直接用子表缓存拼父键(`ensure_player_all_data_async.go:80-93`),拼出来的父键随后被 C++ 载入。子表缓存一旦比库旧,就等于把玩家进度回档。
  - 正向切换时,目标必须没有 `PlayerDatabase*:*`(§5.2 把它列为「目标必须为空」)。
  - 回退时,核心上还留着切换前的子表缓存(TTL:K8s 3600s,缺省 24h),它们没有收到切换期间的回写,比库旧。所以回退必须先在核心上删掉这些键,或者等满一个 TTL 再切回(§5.8)。

#### 3.2.5 留在核心 `redis` 的部分

- **留下的理由**:§2.3 的闭包、§2.4 的 C++ 单连接顺序、CZ-2 和 D12 两条契约。
- **核心不是缓存**。上面有两类只存在于 Redis 的权威数据:
  - 账号目录 `account:{account}`(`loginlogic.go:318-320`:「不是可重新加载的缓存」)和 `player_to_account`;
  - 写回式的 `PlayerAllData` 最新态:Kafka 落库之前,只有这一份。
  所以 noeviction 和 AOF 都不能降。

### 3.3 被否决的切法

| 切法 | 否决理由 |
|---|---|
| 「会话与锁」单独成实例 | 会被 login 身份补齐、外观修复、scene_manager 落点 CAS、C++ guard 这几段脚本切开(§2.3)。要拆,只能把它们改成跨实例两阶段,原子性就没了 |
| 「PlayerAllData 父键」单独成实例 | C++ guard Lua 把父键与 `owner_epoch` 原子绑定,这正是 scene-owner-reentry-barrier §6.2 方案 a 的核心;C++ 还只有一条连接 |
| 核心上 Redis Cluster | `redis-ha.yaml:10-13`;`cross-zone-matchmaking.md` §10.0 列的阻塞表 |
| 核心按 zone 拆 | 违反 CZ-2 / D12:访客数据、跨 zone 匹配读位置,都要求物理共享 |
| 映射用只读副本扩读 | go/db 必须读主节点(`go/db/internal/config/config.go:57-58`) |
| 控制面单独成第四个实例 | 备选,见 §13 第 2 条 |

### 3.4 分批

| 批 | 内容 | 代码 | 数据迁移 | 停服 |
|---|---|---|---|---|
| 1a | 部署能力:`redis-purpose.yaml` 模板、`-RedisSplit`(含 dev_tools 与回档脚本的转发)、拓扑函数与安全闸、契约测试;login `QueueRedis`;`k8s_gate_drain.ps1` 读 QueueRedis;回档守卫补核心族;Java gateway 补密码 | login、PowerShell | 无 | 无(参数缺省为空;§1 第 4 条的两处有意改动除外,行为不变) |
| 1b | 迁移工具 `tools/redis_split`(含 `restore`、`unmark`、墓碑)+ 集群内 Job + 运维手册;merge_zone / data_consistency_check / route_tool 的墓碑检查 | 新 Go module、三个工具 | — | — |
| 1c(运维执行) | 切 queue、mapping;核心清单显式写 noeviction / maxmemory | 无 | 映射拷贝 | queue 无;mapping 冻结建角与合服工具 10–30 分钟(推算);核心显式化要一次核心重启(维护窗口) |
| 2 | go/db `CacheRedis`;merge_zone `-dbctl-redis-*`;dev_tools 转发参数;部署 dbctl | go/db、merge_zone、PowerShell | 控制面拷贝 | 逐 zone 停 go/db(存盘暂停,由 Kafka 缓冲) |
| 3(可选) | login `CacheRedis`;部署 cache;工具改参数 | login、merge_zone、工具 | 无(缓存不拷) | 全服一拍:停入口 + 停 go/db |

## 4. 配置改动点

### 4.1 逐服务

| 服务 / 工具 | 句柄 | 现在 | 第 1 批后 | 第 2 批后 | 第 3 批后 | 改动类型 |
|---|---|---|---|---|---|---|
| C++ gate / scene / battle | `zoneredis` | redis | redis | redis | redis | 不变;契约测试钉住它永不指向拆出去的实例 |
| login | `Node.RedisClient` | redis | redis | redis | redis | — |
| login | `Node.QueueRedis`(新) | —(= RedisClient) | redis-queue | | | 代码 + 配置 |
| login | `Node.CacheRedis`(新) | — | — | — | redis-cache | 代码 + 配置 |
| go/db | `ServerConfig.RedisClient` | redis | redis | **redis-mapping** | | 配置 |
| go/db | `Placement.Redis` | (= RedisClient) | **redis-mapping**(显式写) | 不写(= RedisClient = redis-mapping) | | 配置 |
| go/db | `ServerConfig.CacheRedis`(新) | — | — | redis(显式写) | redis-cache | 代码 + 配置 |
| data_service | `MappingRedis` | redis | redis-mapping | | | 配置 |
| data_service | `DevRedis` / `Regions` | redis | redis | | | 不变(父键在核心) |
| guild | `MergeMarkerRedis` | (K8s 无清单) | 写清单时指向 redis-mapping | | | 配置(随 guild 清单) |
| player_locator、scene_manager、match、trade、friend、chat 的共享段 | — | redis | 不变 | | | — |
| Java gateway(按 zone 部署) | `spring.data.redis` | redis(无密码) | redis-queue + password(口令非空时才写) | | | 配置 |
| Java auth | `spring.data.redis.host` | redis | redis | redis | redis | 不变;契约测试钉住 |
| merge_zone | `-mapping-redis-*` | redis | redis-mapping | | | 运维参数 |
| merge_zone | `-dbctl-redis-*`(新) | — | — | redis-mapping | | 代码 |
| merge_zone | `-notice-redis-*` | redis | redis | redis(只剩合服提示、session、缓存失效) | 子表缓存失效改走新参数 `-cache-redis-*` | 代码 |
| `k8s_gate_drain.ps1` | 读 login ConfigMap | `Node.RedisClient` | 改读 `Node.QueueRedis`,缺省回落 | | | 代码 |
| `k8s_zone_rollback.ps1` | `-RedisHost`;新增 `-RedisSplit` | | 守卫模式补上核心族;转发 `-RedisSplit`;停服前真跑拓扑闸 | | | 代码 |
| data_consistency_check、route_tool、merge_zone | 映射句柄 | | 连到带墓碑的实例即拒绝(第 1b 批) | | | 代码 |
| data_consistency_check | `-redis-addr` | redis | redis-mapping | | | 运维参数 |
| route_tool | 读 data_service yaml | | 跟随 MappingRedis | | | — |
| go/db `cmd/verifier`、`cmd/data_stress`;robot `data_stress`;data_service `cmd/debug_fetch` | 读缓存 | redis | | | redis-cache | 运维参数 |

### 4.2 新配置键、关键类型与函数

**login(第 1a 批)**

```go
// go/login/internal/config/config.go(NodeConfig,:405-414)
// QueueRedis 是排队 / 准入 / gate 排空标记所在的 Redis(shared-redis-split-by-purpose.md §3.2.1)。
// 整段不写 = 复用 RedisClient,而且是同一个 *redis.Client(不多开连接池),行为与拆分前逐字相同。
// 用指针 + optional 的原因:go-zero 不会为没出现的 optional 嵌套结构回填内层字段
// (同 go/db/internal/config/config.go:24-30 的说明)。段一旦出现,内层字段就按 RedisConf 的规则全部必填,
// 所以 ConfigMap 必须整段写出。
QueueRedis *RedisConf `json:"QueueRedis,optional"`
```

```go
// go/login/internal/svc/servicecontext.go
type ServiceContext struct {
    RedisClient *redis.Client
    // QueueRedis:没配置时与 RedisClient 是同一个指针。只给 loginqueue 和 gate 排空使用。
    QueueRedis *redis.Client
    // ...
}

// newRedisClient 从 :107-120 抽出来,给两个句柄共用。Ping 失败就 panic,与现状同口径;
// 错误信息带句柄名和 Host,不带口令。
// redis.Options 只填 Addr / Password / DB / DisableIndentity,与今天逐字相同。
// **不**顺带传 Dial / Read / WriteTimeout(§2.5 第 8 条):传进去会让核心 client 的 DialTimeout 从 5s 变 3s,
// 那是与拆分无关的行为变化,另行登记,不夹带(AGENTS §11.2)。
func newRedisClient(ctx context.Context, name string, c config.RedisConf) *redis.Client

// newQueueRedis:c == nil 时原样返回 core(同一个指针,不多开连接池);否则 newRedisClient(ctx, "queue", *c)。
// 单测 TestNewQueueRedis_NilConfigSharesPointer 钉的就是这个函数。
func newQueueRedis(ctx context.Context, core *redis.Client, c *config.RedisConf) *redis.Client
```

- **注入点**(loginqueue 包本来就接收 `*redis.Client` 参数,所以**包本身不改**):
  - `initLoginQueue` 里的 `gateWatcherCapacityProvider.rdb`(`servicecontext.go:430-435`);
  - `loginqueue.New`(`:442-447`);
  - `loginqueue.NewDispatcher`(`:468-480`);
  - `startGateDrainMonitor` 里的 `StartGateDrainMonitor`(`:524`)。
- **启动日志**:`newQueueRedis` 之后打一行 logx `[Redis] core=<host>/db<n> queue=<host>/db<n>|shared`;启动横幅(`go/login/login.go:466` 的 `redis:` 行)后面补一行 `queue:`,值同上。§5.3、§10 的「横幅」指的是这两处之一,所以 `login.go` 进第 1a 批文件清单。
- **关闭**:与 RedisClient 同口径,不显式 Close(现在两个服务都没有显式关闭 Redis)。

**login(第 3 批)**

- 新键:`` CacheRedis *RedisConf `json:"CacheRedis,optional"` ``。
- 用处:
  - `dispatcher.NewTaskResultDispatcher(s.CacheRedis, …)`(`servicecontext.go:202`);
  - `dataloader.EnsurePlayerAllDataInRedisAsync(ctx, parentRC, cacheRC, …)`:签名加一个参数,父键的 EXISTS / SET 仍走核心(`ensure_player_all_data_async.go:57`、`:103`、`:148`),子表的 GET 走缓存(`:84`);
  - `sync_loader.go` 同口径;
  - 调用点 `entergamelogic.go:380` 附近。

**go/db(第 2 批)**

```go
// go/db/internal/config/config.go(ServerConfig,:69-75)
// CacheRedis 是玩家子表缓存回写与读结果(task:result)所在的 Redis,必须与 login 读它们的实例相同。
// nil = 复用 RedisClient(与拆分前逐字相同)。
// 缓存 TTL 仍取 RedisClient.DefaultTTLSeconds,不新增第二个 TTL 来源。
CacheRedis *CacheRedisConfig `json:"CacheRedis,optional"`

type CacheRedisConfig = PlacementRedisConfig // 同一形状:Hosts 必填,Password / DB 可选
```

- **svc**:新增 `CacheRedis *redis.Client`,没配置时等于 `RedisClient`。写法同 `svc/servicecontext.go:35-48`:连不上只记 ERROR、不拒启。这与 `RedisClient` 同口径:回写失败的写任务不推进游标,留在重试里(`key_ordered_consumer.go:836-843`)。
- **kafka 包**:
  - 构造函数改为 `NewKeyOrderedKafkaConsumer(cfg, redisClient, cacheRedis, placementRedis, stores)`(`:867`;调用点 `go/db/db.go:116-121`)。
    - `cacheRedis == nil` 与 `placementRedis == nil` 同口径返回错误(fail-closed),缺省值只在 svc 层回填,构造函数不替调用方猜。
    - 既有用例 `placement_route_test.go:891-895` 按四参数调用,要同步改成五参数,并补一条「cacheRedis 为 nil 时报错」。
  - consumer 结构(`:68`)和 worker 结构(`:108`)各加字段 `cacheClient redis.Cmdable`;consumer 在 `:918` 赋值,建 worker 时(`:962`)传下去。
  - **共享缓存回写和读结果发布的唯一入口是自由函数 `processTaskWithoutLock(ctx, redisClient, task, route)`(`:1984`)**,它把参数原样传给 `runDBOp`(`:2012`)。`runDBOp` 里没有 `w`,不能写成 `runDBOp(ctx, w.cacheClient, …)`。正确改法:
    - 把 `processTaskWithoutLock` 的第二个参数改名为 `cacheClient`(函数体只把它交给 `runDBOp`,不做别的事);
    - 三个调用点都改传缓存句柄:`processDBTask` 的离线扩分区分支(`:1817`)和正常分支(`:1840`)传 `w.cacheClient`,`tryLockAndProcess`(`:1981`)传 `worker.cacheClient`;
    - 同一函数里读写扩分区状态(`:1820`、`:1832`)和拿锁的句柄仍是 `w.redisClient` / `worker.locker`,不动。
  - `failReadTask` 里改为 `publishTaskResult(w.ctx, w.cacheClient, …)`(`placement_route.go:334-335`);它后面的 `ackRetryReceipt`(`:342`)属于控制面,仍走 `w.redisClient`。
  - 游标、epoch、排序锁、重试、扩分区状态仍走 `redisClient`。
  - 改完后在 go/db 里 grep `publishTaskResult(`、`publishDBOpResult(`、`writeBackDBCache(` 的所有调用点,确认每个都拿到缓存句柄;单测按三条路径各覆盖一次(§8)。
- **启动横幅**:`go/db/db.go:226-231` 补一项 cache 句柄。

**merge_zone(第 2 批)**

- 新参数 `-dbctl-redis-addr` / `-dbctl-redis-password` / `-dbctl-redis-db`,缺省取 `-notice-redis-*` 的值(向后兼容)。
- 走新句柄的:
  - preflight 的重试 / 死信队列检查(`preflight.go:260-270`,改用 `d.dbctlRdb`);
  - 审计 `kafka_db_task_queues`(`audit_checks.go:153-165`;`audit_resources.go:228-251`);
  - relocate R2 等排序锁(`relocate_run.go:339`、`:478`)。
- 仍走 notice 句柄的:`player:session` 与组队检查(`preflight.go:296`、`:308`;`audit_checks.go:82-85`),合服提示(`unmerge.go:231`)。
- 第 3 批:`invalidatePlayerCaches`(`player_rows.go:718-745`)拆成两半:父键走 notice 句柄,子表走 `-cache-redis-*`。

**`k8s_deploy.ps1` 参数**

| 参数 | 类型 / 缺省 | 说明 |
|---|---|---|
| `-RedisSplit` | string[];ValidateSet `queue`、`mapping`、`dbctl`、`cache`;所有档位缺省 `@()` | 空 = 生成的 ConfigMap 与 infra 与今天逐字节相同 |
| `-RedisQueueMaxMemory` / `-RedisMappingMaxMemory` / `-RedisCacheMaxMemory` | string,形如 `512mb`;dev 档缺省 `128mb` / `256mb` / `256mb` | release 档(ReleaseProfile 不是 dev,或 OpsProfile 不是 custom)启用了对应用途却没给 → 拒绝部署 |
| `-RedisCoreMaxMemory` | string;缺省空 = 核心清单不变 | 给了就改核心 StatefulSet 的命令行,会重启 redis-0,必须同时给 `-ConfirmRedisCoreRestart` |
| `-ConfirmRedisSplitSwitch` | string[] | 线上已有 ConfigMap 的用途要改地址时必须列出,并通过 §4.4 的迁移标记检查 |
| `-ConfirmRedisCoreRestart` | switch | 见上 |

**`-RedisSplit` 是每次调用都要带的参数,漏带 = 想回到共享拓扑**。部署闸(§4.4)会把这种「地址要变」拦下来,不会静默切回;但下面这些包装入口必须原样转发,否则会在半路被闸拒绝:
- `dev_tools.ps1` 的 `k8s-infra-up`、`k8s-zone-up`、`k8s-all-up`、`k8s-release-zone`、`k8s-release-all`;
- `k8s_zone_rollback.ps1`:Step 6 用固定的 `$upArgs` 调 zone-up(`:771-790`),今天没有 `-RedisSplit`。不补的话,拆分后的回档会在**已经停服、MySQL 已经回档之后**的 Step 6 被闸拒绝。它停服之前的静态预检是 `-DryRun`(`:843-848`),DryRun 不查集群,拦不住。所以回档脚本要:(1) 新增 `-RedisSplit` 并转发;(2) 在停服之前、只读地跑一次 `Assert-RedisTopologyChangeIsSafe`(真查集群),不一致就在停服前拒绝。

拓扑只在一个函数里算,写法仿照 `Get-KafkaTopologyFor`(`k8s_deploy.ps1:1049`):

```powershell
# 返回 @{ Core; Queue; Mapping; DbCtl; Cache },每项是 @{ Service; Host; Port; HostPort },
# 例如 @{ Service='redis-queue'; Host='redis-queue.<ns>'; Port=6379; HostPort='redis-queue.<ns>:6379' }。
# 分开给 Host 与 Port,是因为 gateway(host / port 两个键)和 C++ zoneredis(host / port)不吃 "host:port"。
# 规则:dbctl 必须同时启用 mapping,否则 throw "dbctl 必须与 mapping 同实例(§3.2.3)";
# DbCtl = Mapping;没拆的用途 = Core。
function Get-RedisTopologyFor { param([string[]]$Split, [string]$InfraNamespace) ... }
```

所有 ConfigMap 生成器只从这个函数取地址:
- login(`:3583-3590`):`RedisClient` = Core;Queue ≠ Core 时整段写 `QueueRedis`;Cache ≠ Core 时写 `CacheRedis`。
- go/db(`:3456-3465`):`RedisClient` = DbCtl;Mapping ≠ DbCtl 时显式写 `Placement.Redis`;Cache ≠ DbCtl 时写 `CacheRedis`。
- data-service:`MappingRedis` = Mapping;`DevRedis` / `Regions` = Core。
- gateway:`spring.data.redis` = Queue;注入的口令非空时才写 `password`(dev 档渲染逐字节不变)。
- auth、C++ 的 `zoneredis`:恒为 Core。

### 4.3 K8s 清单

- **新增模板 `deploy/k8s/manifests/infra/redis-purpose.yaml`**,结构照 `redis.yaml`:
  - 对象:Service `__REDIS_NAME__`、headless `__REDIS_NAME__-headless`、PDB `__REDIS_NAME__-pdb`(minAvailable 1)、StatefulSet `__REDIS_NAME__`(replicas 1)。
  - 标签与 selector 一律用 `app: __REDIS_NAME__`。核心 Service `redis` 的 selector 是 `app: redis`(`redis.yaml:22-30`),新 Pod 不能带这个标签,否则会被挂到核心 Service 后面。契约测试检查渲染结果里没有 `app: redis` 的新对象。
  - 镜像 `redis:7.2`,与 `redis.yaml:83` 一致;WAITAOF 要求 ≥ 7.2。
  - 命令:`--appendonly yes --appendfsync everysec --maxmemory __REDIS_MAXMEMORY__ --maxmemory-policy noeviction`;口令可选,做法同 `redis.yaml:90-104`。
  - **环境变量名必须叫 `REDIS_PASSWORD`**,来自 Secret `redis-auth`(optional: true)。`k8s_gate_drain.ps1:630-640` 和 §4.4 的标记检查都是 exec 进 Pod 后读这个变量。
  - 探针照搬 `redis.yaml:112-135`。
  - requests 与 limits 的内存都取 `__REDIS_MEM__` = 2 × maxmemory(AOF 重写时 fork,写时复制最坏会翻倍);CPU `__REDIS_CPU__`;PVC `__REDIS_STORAGE__`。
- **怎么 apply**:
  - 在 `Apply-Infra` 里 `redis.yaml` 之后,按 `Get-RedisTopologyFor` 启用的用途循环渲染(占位替换写法同 `:5835-5851`)。
  - 复用 `:5854-5859` 的「有未替换占位就拒绝」检查。
  - Secret `redis-auth` 已经在 `redis.yaml` 那个分支里先建好(`:5789-5812`)。
  - `-WaitReady` 时逐个 `Wait-ForStatefulSetReady`(同 `:5877-5879`)。
- **不删已有对象**:没启用的用途,脚本不会删掉已存在的 `redis-queue` 等对象。回退时数据还在里面,删不删由人决定。
- **核心 `redis.yaml`**:
  - 第 1c 批显式写 `--maxmemory-policy noeviction`、`--maxmemory` 和 limits。`:136-143` 的注释原本就写明要等压测数据再定。
  - 改 Pod 模板 = redis-0 重启 = 全服登录中断(`redis.yaml:17-20`)。
  - 所以命令行加占位 `__REDIS_CORE_ARGS__`,resources 段加占位 `__REDIS_CORE_LIMITS__`。不给 `-RedisCoreMaxMemory` 时两者都渲染成空串,渲染结果必须与今天的清单逐字节相同(契约测试用 golden 文件钉住)。给了却没给 `-ConfirmRedisCoreRestart` 就拒绝。
    - 占位必须**行内**放置:命令行里两处 `exec redis-server --appendonly yes`(有口令、无口令两个分支,`redis.yaml:93-97`)都要加,写成 `--appendonly yes__REDIS_CORE_ARGS__`,替换值以空格开头;`__REDIS_CORE_LIMITS__` 接在 `memory: 512Mi` 行尾,替换值以换行开头。单独占一行的占位替换成空串后会留下空行,golden 比对就不再逐字节相同。
  - 不重启的过渡做法(供 §13 第 4 条参考):Redis 缺省就是 noeviction(核心命令行没传 `--maxmemory-policy`),`maxmemory` 可以先用 `CONFIG SET` 在线设上,不动 Pod 模板。代价是重启后丢失,要等下一次计划内维护窗口再把清单改成显式值。
- **`redis-ha.yaml` 不动**:它的 apply 路径在脚本里不存在(`tidb-phase1-and-launch-readiness.md` §9.5)。

### 4.4 部署脚本的安全闸

`Assert-RedisTopologyChangeIsSafe`,写法仿照 `Assert-KafkaTopologyChangeIsSafe`(`k8s_deploy.ps1:1130`)。DryRun 时不查集群。

为了能单测,读集群的部分以 scriptblock 注入(同 `k8s_zone_rollback.ps1` 的 `Assert-RedisFlushTargetIsZoneLocal -Probe` 写法):`-ReadLiveConfigMap`、`-ReadMarker`、`-ServiceExists`、`-ScanCount`。

1. 读线上 ConfigMap,求出各用途的现地址:
   - queue:login 的 `Node.QueueRedis.Host`,没有就取 `RedisClient`;gateway 的 `spring.data.redis.host`。两者不一致就拒绝(说明上次切换没做完);
   - mapping:data-service 的 `MappingRedis.Host`;
   - dbctl:db 的 `ServerConfig.RedisClient.Hosts`;
   - cache:db 的 `ServerConfig.CacheRedis.Hosts`,没有就取 `RedisClient`;login 的 `Node.CacheRedis.Host`,没有就取 `Node.RedisClient.Host`。两者不一致就拒绝。
   ConfigMap 不存在(新 zone、新集群)时不检查地址变化,但第 4 条照查。
2. 地址要变的用途,必须出现在 `-ConfirmRedisSplitSwitch` 里。漏带 `-RedisSplit` 的例行部署会在这一步被拦下,报错里写明「线上已拆分为 X,请带 -RedisSplit X」。
3. mapping 和 dbctl 还要求**目标实例**上已有迁移标记 `redis-split:migrated:<purpose>`(dbctl 按 zone:`…:dbctl:zone:<z>`),标记里的 `source` 等于现地址、`verified` 为 true。
   - 读法:`kubectl exec svc/<target> -- redis-cli GET`,口令取容器内的 `REDIS_PASSWORD`,做法同 `k8s_gate_drain.ps1:630-640`。
   - 没有标记就拒绝,提示先跑 `tools/redis_split`。
   - **标记必须比源上的同名标记新**:回退时源(比如核心)上也会被 `mark` 写一份。目标标记的 `verified_at_ms` 不大于源标记的,说明目标上是回退之前的旧数据,拒绝。否则「正向迁移 → 回退 → 再正向但忘了重新拷贝」会凭第一次留下的标记通过,把读方切到一份旧映射上。
   - cache 用途要求目标上有 `-skip-copy` 标记,且标记写入时目标上没有 `PlayerDatabase*:*`(§5.2)。
4. 每个启用的用途,目标 Service 必须存在(`kubectl get svc <name> -n <infra>`)。queue 没有迁移标记,不查这条的话 zone-up 会把 login 指向一个不存在的实例,login Ping 失败 panic、CrashLoop。
5. queue 还要求:
   - 核心上 `SCAN MATCH gate:*:draining` 结果为空,即没有正在进行的排空;
   - 线上所有 zone 的 login ConfigMap 里 `Queue.Enabled` 都不是 true(§3.2.1 约束 2)。
6. C++ 的 `zoneredis` 和 auth 永远等于 Core:生成器里写死取 Core,契约测试再钉一次。

DryRun 不调这个函数(同 Kafka 闸)。所以凡是「先 DryRun 预检、后真执行」的流程(典型是 `k8s_zone_rollback.ps1`),要在产生任何改动之前单独真跑一次本函数(只读),见 §4.2。

另新增 `dev_tools.ps1 -Command k8s-redis-split-status`:
- zone 列表用 `Resolve-ZonesConfigPath`(支持 `-ZonesConfigPath`、json 与 yaml,`k8s_deploy.ps1:5449-5451`、`:5916`),不写死 `zones.json`;
- 对每个 zone 打印各用途的现地址:ConfigMap 里的值,**以及**每个相关 Deployment(login、gateway、data-service、db)的 rollout 是否完成(带短超时的 `kubectl rollout status` 的结果,以及是否还有旧 ReplicaSet 的 Pod)。只看 ConfigMap 不够:ConfigMap 改了不等于旧 Pod 都退场了,旧 Pod 还在往旧实例写;
- queue、mapping、cache 在 zone 之间不一致,或任一 rollout 没完成,以退出码 1 结束;dbctl 按 zone 迁移(§5.5),各 zone 不一致只打印、不算失败。
- mapping、cache 的切换步骤和所有 purge 之前都以它为闸(§5.4、§5.6、§5.8)。

### 4.5 契约测试

新用例都加在 `tools/scripts/tests/k8s_deploy_contract.tests.ps1`,清单见 §8。
既有的 `:839` 用例今天只检查缺省渲染(`$devOut`)。要让它「在所有拓扑下都通过」,把用例体抽成一个函数(入参是渲染输出),原用例照旧对 `$devOut` 调一次,新用例再对 `-RedisSplit queue,mapping`、`mapping,dbctl`、`mapping,dbctl,cache` 的渲染各调一次。断言本身一字不改。

### 4.6 运维脚本

- **`k8s_gate_drain.ps1`**:`Node.QueueRedis.Host` / `DB` 存在就用它,否则用 `Node.RedisClient.*`(改 `:312-330`、`:368`)。新句柄 DB 非法时与现状同口径,拒绝。
- **`k8s_zone_rollback.ps1`**:
  - `Get-SharedRedisGuardPatterns`(`:633-642`)补上核心族:`account:*`、`player_to_account:*`、`player:session:*`、`player:*:owner_epoch`。
    - 原因:第 1 批 purge 之后核心上不再有映射族,第 2 批 purge 之后也不再有游标,原有的守卫模式在核心上就不再命中。不补的话,对核心 FLUSHDB 会被放行,而那会清掉全服未落库的最新存档和账号目录。
    - **不加 `PlayerAllData:*`**:Step 4 的用途写明是「清 zone 的数据缓存、从 MySQL 重建」(`:5`、`:32`、`:922`),zone 私有缓存里有 `PlayerAllData:*` 是正常的,把它列进守卫会拦掉本来合法的目标。核心上一定有 `account:*`,所以不加它也拦得住核心。
    - 今天核心上已有映射族,守卫本来就拒绝,补上之后看不到行为变化;所以随第 1a 批一起补。
    - 参数注释(`:211-213`)和拒绝文案(`:695`)里列举的键类要同步补上「账号目录、会话、归属 epoch」。
  - 新增 `-RedisSplit` 参数并转发给 Step 6 的 zone-up;停服之前真跑一次 `Assert-RedisTopologyChangeIsSafe`(§4.2、§4.4)。
- **`dev_tools.ps1`**:新增 `k8s-redis-split-status` 和 `k8s-redis-wipe`(§5.9);`-RedisSplit`、`-ConfirmRedisSplitSwitch`、各 `-Redis*MaxMemory`、`-ConfirmRedisCoreRestart` 原样转发给 §4.2 列出的部署入口和 `k8s-zone-rollback`。第 2 批再转发 merge_zone 的 `-dbctl-redis-*`。
- **`stress_snap.ps1`**:加 `-RedisTargets` 参数(§6.4)。

## 5. 迁移

### 5.1 原则

- **固定顺序**:停写 → 拷贝 → 校验并打迁移标记 → 切换读写方 → 补齐冻结期漏网的写(delta)→ 解冻 → 浸泡 → 清理旧实例上的键(purge)。
- **不做双写**:
  - 映射和控制面的写都是带复核的 Lua(围栏检查 + 写入、set-if-newer、CAS),拆成两个实例各写一次,就不再原子。
  - 一边成功一边失败时,没有任何仲裁者能判定哪份对。
- **不做双读**(先读新实例,读不到再读旧实例):
  - 对映射来说,这正是 D1 要堵的「缺键 ≠ 无记录」漏洞。旧实例上过期但看起来合理的值会被当成权威。
  - 对缓存来说可以接受,但只为一次性的切换写代码不值得(YAGNI),冷读就够了。

### 5.2 迁移工具 `tools/redis_split`(第 1b 批)

- **形态**:独立 Go module,与 merge_zone 同口径,只依赖 go-redis v9.7.3(`tools/merge_zone/go.mod`),不引入 miniredis。
  - 单测通过接口注入假实现。
  - 真 Redis 测试打 `integration` tag。
- **族表 `families.go` 是唯一出处**:

```go
type Policy int

const (
    PolicyCopy        Policy = iota // 拷贝,保留 PTTL
    PolicySkip                      // 不拷:缓存、心跳、会自动重建的键
    PolicyMustBeEmpty               // 迁移时必须不存在(锁);存在说明写者没停干净 → 拒绝
)

type Family struct {
    Purpose  string   // queue | mapping | dbctl | cache
    Name     string
    Patterns []string
    Policy   Policy
}
```

- **执行位置**:Service 地址只在集群内可解析。merge_zone 今天是在运维机上 `go run`、靠参数传地址(`dev_tools.ps1:945-1011`)。本工具同样可以经 `kubectl port-forward` 跑,但百万级键过 port-forward 的吞吐没测过,直接决定 §5.4 的冻结时长。所以:
  - 第 1b 批同时给出一个一次性 Job 清单(镜像同 go-svc 的构建方式),在 infra namespace 里跑,口令从 Secret `redis-auth` 注入环境变量;
  - kind 演练(§10)分别记录 port-forward 与 Job 两种方式的拷贝耗时,冻结窗口按实测定。
- **扫描方式**:一次 SCAN 遍历(COUNT 1000)、客户端按族分类,不按族各扫一遍。核心上的键绝大多数是 `PlayerAllData`、会话这类不相关的键,MATCH 是在服务端遍历之后才过滤的,按族各扫一遍等于把整个键空间扫 N 次。
- **各用途的族**:

  | 用途 | 拷贝 | 不拷 | 必须为空 |
  |---|---|---|---|
  | mapping | `player:zone:*`、`player:placement:*`、`player:maprev:*`、`merge:in_progress:*`、`merge:merged_into:*` | `db:capability:zone:*`(go/db 30s 内会重写)、`player:name:*`(缓存)、`mapping:route:ready`(见下) | 源与目标上的 `lock:player:*` |
  | dbctl | 按 `-topics` 给出的**完整 topic 名**逐个拼出精确前缀:`consumer:applied:<topic>:*`、`consumer:applied_epoch:<topic>:*`、`kafka:retry:queue:<topic>`、`kafka:retry:processing:<topic>` 与 `kafka:retry:processing:<topic>:*`、`kafka:retry:instances:<topic>`、`kafka:dead:queue:<topic>`、`kafka:expand:status:<topic>` | — | `distributed:lock:kafka:ordering:<topic>:*`;全服的 `distributed:lock:kafka:consumer:lock:*`(不含 topic,§3.2.3) |
  | queue | — | `rl:*` | `gate:*:draining`、`queue:*`、`admit:*`、`admitted:*`(排队开着时要带 `-allow-drop-queue`) |
  | cache | — | 全部 | **目标上**的 `PlayerDatabase*:*`、`task:result:*`(§3.2.4) |

  - **dbctl 不接受通配的 topic 过滤**。`db_task_zone_1` 当子串或前缀用,会同时命中 `db_task_zone_10`…`_19`、`_100`… 和换代后的 `db_task_zone_1_g2`(`go/db/internal/config/config.go:299-310`)。拷错了还不算最糟:后面 purge 会把这些仍在核心上工作的 zone 的游标删掉,顺序守卫和 epoch 守卫随之失效。所以参数叫 `-topics`,值是逗号分隔的完整 topic 名,工具对每个名字校验 `^db_task_zone_\d+(_g\d+)?$`,并且只拼上表的精确前缀。一个 zone 有多代 topic 时要全部列出。
  - **`mapping:route:ready`(D1 第 1b 批之后才存在)**:不拷。切换读方之前,在**源**上 DEL 它,purge 也把它列进去。源不再是权威之后还挂着「副本完整可信」的标记,等于给回退埋雷:回退时源的 `run_id` 没变,对账 leader 会把这份缺了切换期间新写的旧副本当成可信(proposal `:260-272` 只在身份变化时撤标记)。
  - 迁移标记 `redis-split:*` 本身不属于任何族,verify 和 purge 都跳过它。

- **子命令**:
  - **`plan -src A -dst B -purpose P`**:只读。对每个族做 SCAN 计数;「必须为空」的族不为空就退出码 1;报告目标实例上已有的本用途键。
  - **`copy`**:
    - 前置条件是 `plan` 通过。目标上已有本用途的键就拒绝,除非带 `-resume`(此时只补缺)。
    - 逐批 SCAN(COUNT 1000),按类型读:`GET + PTTL` / `LRANGE` / `ZRANGE WITHSCORES` / `HGETALL` / `SMEMBERS`;用 pipeline 写:`SET PX` / `RPUSH` / `ZADD` / `HSET` / `SADD + PEXPIRE`。
    - 开始和结束各取一次源实例 `INFO server` 的 `run_id`。拷贝期间源实例重启过就以退出码 3 结束。
  - **`delta`**:只补「源有、目标没有」的键。两边都有但值不同时,记为冲突并以退出码 1 结束。目标已经是权威,绝不覆盖。用于收住冻结失效窗口里写进旧实例的建角(§5.4 第 6 步)。
  - **`verify`**:双向比较三种差异:源有目标无、目标有源无(只看本用途的族)、值不同。
    - 值按类型做逻辑比较,不比 DUMP 字节:同一个值在两边的内部编码可能不同。
    - PTTL 差超过容差(缺省 5s)只告警。
  - **`mark`**:`verify` 通过后,在目标上写 `redis-split:migrated:<purpose>[:zone:<z>]`,无 TTL,值为 JSON `{purpose, source, source_run_id, keys, verified, verified_at_ms, tool_version}`。cache 用途用 `-skip-copy` 写一个「已确认不拷」的标记,写之前复核目标上没有 `PlayerDatabase*:*`。部署闸只认这个标记,并要求它比源上的同名标记新(§4.4 第 3 条)。
    - `mark` 同时在**源**上写墓碑 `redis-split:moved:<purpose>[:zone:<z>]`,值为 `{to, at_ms}`,并删掉**目标**上的同名墓碑(目标从这一刻起是权威;回退之后再正向迁移时,目标上会留着上一次回退写下的墓碑)。用途见下面「误配防线」。
  - **`unmark`**:删除指定实例上的迁移标记与墓碑。回退完成后,对被放弃的那个实例执行(§5.8)。
  - **`restore -confirm-overwrite`**:回退专用,方向是「新实例 → 旧实例」。
    - 先 UNLINK 目标(旧实例)上本用途「拷贝」类的键(dbctl 按 `-topics` 精确过滤),再做一次 `copy`,然后双向 `verify`。
    - 为什么不能用 `delta`:旧实例上留着切换前的旧值。映射在浸泡期间可能被改写(合服工具解冻后照常运行),只补缺会把这些旧值原样留下;控制面更糟,核心上的旧游标与旧 epoch 比真实值小,go/db 切回之后,重试队列里已经落过库的任务会再落一次,顺序守卫与 epoch 守卫都被绕过,旧数据盖掉新数据。
    - 前置条件与 `copy` 相同:写者已停(冻结 / go/db 缩到 0),「必须为空」的族为空。
- **误配防线(墓碑)**:切换之后、purge 之前,旧实例上的映射与控制面仍在,而且「看起来正常」。运维给 merge_zone 漏传 `-mapping-redis-addr`(它缺省取 `-redis-addr`,`tools/merge_zone/main.go:256`),合服围栏、改映射就会写进没人读的核心:data_service 看不到围栏,合服中的 zone 继续建角,合服报成功却零改动。所以第 1b 批同时改三个读写映射的工具:merge_zone、`tools/data_consistency_check`、`route_tool`,连上映射句柄后先 `EXISTS redis-split:moved:mapping`,存在就拒绝并打印墓碑里的 `to`。第 2 批 merge_zone 新增的 dbctl 句柄(缺省等于 notice 句柄)同理,对本次涉及的每个 zone 检查 `redis-split:moved:dbctl:zone:<z>`;notice 句柄本身合法地指向核心,不做这项检查。
  - data_service 与 go/db 启动时对自己的映射句柄做同样的检查(命中即拒启),能再兜住「ConfigMap 被手工改回」这种绕过部署闸的情形。这要改两个服务的代码,第 1 批就不再是纯配置改动,所以列为 §13 第 15 条待拍板,不默认纳入。
  - **`purge -confirm-purge`**:
    - 只在源实例上删除本用途「拷贝」类的键。
    - 要求目标上的标记存在,且 `verified_at_ms` 早于 `-min-soak`(缺省 24h)。
    - 分批 UNLINK,只打印数量,不打印键名。
    - **这是永久删除,由运维执行。**
- **退出码**:0 成功;1 校验或前置不通过;2 参数错误;3 源在拷贝期间变化,需要重跑。与 `route_tool` 同口径。
- **口令**:只收环境变量名 `-src-password-env` / `-dst-password-env`,做法同 trade 的 `AssetOp.SecretEnv`(`k8s_deploy.ps1:4022-4024`)。口令不进命令行,也不进日志。

### 5.3 排队与限流

前提:L-02 还没打开(排队、限流都关着)。

1. 第 1a 批已部署(参数缺省为空)。
2. `infra-up -RedisSplit queue`:建出 `redis-queue`,不改任何服务。
3. `redis_split plan -purpose queue`:`gate:*:draining` 和 `queue:*` 都为空;所有 zone 的 login `Queue.Enabled` 为 false(部署闸也会查)。
4. 从这一步起到第 5 步结束,**禁止** `k8s_gate_drain.ps1` 和 gate 滚动(§3.2.1 约束 3)。
5. 所有 zone 执行 `zone-up -RedisSplit queue -ConfirmRedisSplitSwitch queue`:login ConfigMap 写入 `QueueRedis`,滚动重启;gateway 也是按 zone 部署的(§2.1),随同一次 zone-up 切到 `redis-queue`。之后跑 `k8s-redis-split-status`,确认各 zone 的 ConfigMap 一致、rollout 都已完成。
   - 逐 zone 执行期间,已切和未切的 zone 的 gateway 各用一个实例上的 `rl:ip:*` 桶,同一 IP 的限额暂时翻倍。限流关着(L-02 之前)时没有影响,这也是「先拆再开 L-02」的另一个理由。
6. 验证:
   - login 启动横幅出现 `queue: redis-queue…`;
   - `k8s_gate_drain.ps1` dry-run 读到的是 `redis-queue`;
   - 核心上 `SCAN MATCH queue:*` 不再增长。

不需要停服。滚动期间同一 zone 的新旧 login 各看一个实例的排空标记,但第 3 步已经保证两边都没有标记,第 4 步保证窗口里不会产生新标记。

### 5.4 映射

**路径 A**(今天就能做,`RouteStore.Mode=redis`):

1. `infra-up -RedisSplit queue,mapping`:建出 `redis-mapping`。
2. **冻结**:
   - 禁止 merge_zone 的一切写模式、backfill、relocate、pin。这与 D1 P1 的冻结窗口是同一份规矩(proposal §7)。
   - 用 killswitch 关掉 `DataService/RegisterPlayerZone`;登录侧表现为建角失败、可重试。
   - killswitch 是 fail-open 的(`go/shared/killswitch/killswitch.go:10-22`),**不能当唯一的闸**,所以后面有第 6 步。
3. `redis_split plan / copy / verify / mark -purpose mapping`:源是核心,目标是 `redis-mapping`。`mark` 同时在核心上留下墓碑 `redis-split:moved:mapping`(§5.2)。如果核心上有 `mapping:route:ready`(D1 第 1b 批之后),在这一步 DEL 掉。
4. 所有 zone 执行 `zone-up -RedisSplit queue,mapping -ConfirmRedisSplitSwitch mapping`。
   - data-service 的 `MappingRedis` 和 go/db 的 `Placement.Redis` 同一批改。
   - 部署闸会检查第 3 步写的标记。
   - 滚动期间新旧副本读的是两个内容相同的实例(冻结中),安全。
5. `k8s-redis-split-status`:所有 zone 的 data-service 和 db 的 ConfigMap 都指向 `redis-mapping`,**并且 rollout 全部完成、没有旧 ReplicaSet 的 Pod**。只看 ConfigMap 不够:没退场的旧 data-service Pod 仍会把漏网的建角写进核心,而它可能写在第 6 步之后。
6. `redis_split delta`:把冻结失效窗口里落到核心的建角补进 `redis-mapping`。出现冲突就停在这一步,由人处理。
7. 再跑一次 `verify`,然后 `mark`(刷新 `verified_at_ms`)。
8. **解冻**:删掉 killswitch 规则。然后观察:
   - 建角成功率和 data_service 的路由计数;
   - go/db 启动日志里的落点行(`go/db/db.go:226-231`);
   - 核心上 `player:zone:*` 不再增长。
9. 浸泡至少 24h 后,先再跑一次 `k8s-redis-split-status`,再 `purge` 核心上的映射族(运维执行,永久删除)。墓碑与迁移标记不删。
   - 这一步之后,任何仍然读核心的组件都会把所有玩家判成「无映射」。表现是大面积、看得见的失败,而不是静默写错库。
   - purge 之前的浸泡期里,误配到核心的 merge_zone / data_consistency_check / route_tool 由墓碑拦下(§5.2「误配防线」)。

停服范围:只有建角冻结,即第 2–8 步,推算 10–30 分钟;百万级键的拷贝在分钟级(取决于执行位置,§5.2,以 kind 实测为准)。已在线的玩家、登录和进场都不受影响,因为读映射的两份副本内容一致。

**路径 B**(D1 第 1b 批和第 3 批的读方开关上线之后):

- 不拷映射。data_service 直接指向空的 `redis-mapping`。
- 对账 leader 从 `player_route` 全量预热,然后挂就绪标记。预热期间 go/db 开着 `RequireRouteReady`,写任务延后,不消耗重试预算(proposal `:239`)。围栏和 merged_into 从库里补。
- 只需要第 2、4、5、8 步。
- **推荐 D1 到位后走路径 B;开服前 D1 没到位就走路径 A**(§13 第 11 条)。

### 5.5 存盘控制面(第 2 批,逐 zone 执行)

前提:
- 从第一个 zone 开始到最后一个 zone 切完,**全服禁止在线扩分区**(§3.2.3),也禁止运行 merge_zone(它的 `-dbctl-redis` 句柄只能指向一个实例)。
- **与 L-09 的关系**:K8s 上所有 zone 的 go/db 共用一个消费组 `db_rpc_consumer_group`(`tidb-phase1-and-launch-readiness.md` L-09,`k8s_deploy.ps1:3437`)。缩容和扩回各触发一次全组 rebalance,**全服**存盘各停几秒,Z 个 zone 一共 2Z 次。建议先做 L-09(消费组按 zone 命名)再做第 2 批;做不到就把每个 zone 的两次 rebalance 都排进维护窗口,并在 robot / 监控里确认每次之后各 zone 的 lag 都追平。

步骤:
1. 部署第 2 批代码。此时 `CacheRedis` 缺省等于 `RedisClient`,行为不变。
2. 把 zone Z 的 go/db 缩到 0。
   - 存盘由 Kafka 缓冲。
   - 父键缺失的玩家(新号首次进场必然如此)进场要冷读,go/db 停着就会撞上 8s 超时(`entergamelogic.go:380`),所以选低峰期,或者配合维护窗口。
3. `redis_split plan / copy / verify / mark -purpose dbctl -topics db_task_zone_Z[,db_task_zone_Z_g<N>…]`(完整 topic 名,§5.2)。
   - 本 zone 的排序锁、全服的扩分区锁必须为空。go/db 正常停机会释放排序锁(`:1945-1951` 用脱离取消的 ctx 释放);被强杀的话要等租约 2 分钟过期。
4. `zone-up -RedisSplit queue,mapping,dbctl -ConfirmRedisSplitSwitch dbctl`:go/db 的 `RedisClient` 指向 `redis-mapping`,`CacheRedis` 指向 `redis`。
5. go/db 起来后检查:
   - 横幅显示 cursor / lock 在 `redis-mapping`、cache 在 `redis`;
   - 消费恢复,本 zone lag 追平;
   - `redis-mapping` 上 `SCAN MATCH consumer:applied:db_task_zone_Z:*` 的数量在增长,核心上同一 pattern 的数量不再增长。**不要写成 `*db_task_zone_Z*`**:Z=1 时它还会命中 zone 10–19 和换代 topic。
6. 所有 zone 做完并浸泡后,先跑 `k8s-redis-split-status`,再逐 zone `purge -topics …` 核心上的控制面键。

### 5.6 子表缓存与读结果(第 3 批)

需要全服一拍完成:
1. 停入口:gate 缩到 0,或发维护公告。
2. 等 db_task 的 lag 和在途的 `task:result` 都清零。
3. `redis_split plan -purpose cache`:目标 `redis-cache` 上没有 `PlayerDatabase*:*` 与 `task:result:*`(§3.2.4;实例是新建的就天然满足,曾经用过、回退过的实例必须先清空)。然后 `mark -skip-copy`。
4. 所有 zone 的 go/db 和 login 同一批执行 `-RedisSplit …,cache -ConfirmRedisSplitSwitch cache`。缓存不拷。
5. `k8s-redis-split-status` 通过(含 rollout 完成)后开入口。

### 5.7 核心清单显式化(随第 1c 批的维护窗口)

- 执行 `-RedisCoreMaxMemory X -ConfirmRedisCoreRestart`,redis-0 会重启。
- 重启时重放 AOF;startupProbe 最多给 5 分钟(`redis.yaml:111-120`)。这段时间全服登录中断。
- 在线会话和位置随 AOF 保留,不会丢。这一点与 2026-09-19 从 emptyDir 切到 PVC 的那次不同。

### 5.8 回退

- **原则**:切换之后,新写只落在新实例上,旧实例上留着的是切换那一刻的旧值。**回退必须用新实例的数据整体覆盖旧实例,不能只改地址,也不能只补缺。**所以回退一律用 `redis_split restore -confirm-overwrite`(先清目标上的本用途键再拷,§5.2),不用 `delta`。purge 前后做法相同。
- **queue**:
  - 排队关着时:去掉 `queue` 重新部署即可,没有数据要搬。约束同 §5.3(禁排空与 gate 滚动,直到所有 zone rollout 完成)。
  - 排队已经打开(L-02 之后)时:逐 zone 回退会重演 §3.2.1 约束 2 的双 leader 与双队列;排位与在途准入随实例丢失。必须先把各 zone 的 `Queue.Enabled` 关掉、等 `admitted:*` 过期(AdmitTTL + 1min,默认 2 分钟),再回退。部署闸的 `Queue.Enabled` 检查对回退方向同样生效。
- **mapping**:
  1. 冻结(同 §5.4 第 2 步)。
  2. 如果 `redis-mapping` 上有 `mapping:route:ready`,先记下;核心上的同名键一律 DEL。
  3. `redis_split restore -purpose mapping`:源是 `redis-mapping`,目标是核心。
  4. 双向 `verify`,然后在核心上 `mark`(这一步会在 `redis-mapping` 上写墓碑)。
  5. 部署时去掉 `mapping`,带 `-ConfirmRedisSplitSwitch mapping`;部署闸会检查核心上的标记,并确认它比 `redis-mapping` 上的标记新。
  6. `k8s-redis-split-status` 通过(含 rollout 完成)后,对 `redis-mapping` 执行 `unmark`(删掉第一次正向迁移留下的标记,墓碑保留)。
  7. 解冻。
- **dbctl**:按 §5.5 逐 zone 反向执行,把第 3 步换成 `restore -purpose dbctl -topics …`。前提相同:该 zone 的 go/db 缩到 0、全服不扩分区、不跑 merge_zone。**禁止用 `delta`**:核心上的旧游标和旧 epoch 比真实值小,只补缺会让重试队列里已经落过库的任务再落一次,旧数据盖掉新数据(§5.2 `restore`)。
- **cache**:按 §5.6 反向执行,不拷;但切回之前必须在核心上 UNLINK 全部 `PlayerDatabase*:*`(或者等满一个 `DefaultTTLSeconds`,K8s 为 3600s)。核心上的这些键停在切换那一刻,比库旧,login 会用它们拼父键,等于回档(§3.2.4)。
- **时限**:没有硬时限,数据都还在 Redis 里。
- **再次正向迁移**:被放弃的实例上还留着旧数据。`copy` 遇到目标有本用途的键会拒绝,所以要先对它 `purge`(此时它是源侧),再按 §5.4 / §5.5 从头做;部署闸的「目标标记必须比源标记新」也会拦住漏做拷贝的情形。
- **代码回退**:第 1a、2、3 批的代码在 `-RedisSplit` 为空时与今天逐字相同(§1 第 4 条列出的两处有意改动除外)。回退代码之前,先把拓扑回退到共享。

### 5.9 与 AGENTS §6.2 压测清库纪律的关系

- §6.2 第 2 步「redis-cli FLUSHALL」和第 6 步「每次登录压测前把 redis 数据全部删除」,在拆分之后指的是**拓扑里的每一个实例**,再加 `redis-match-cluster`。
- **漏掉 `redis-mapping` 的后果最严重**:
  - 如果清了 broker 数据目录、topic 重建,offset 会从 0 重新开始。旧的 `consumer:applied:*` 游标比新 offset 大,go/db 就会把新写判成「已落库」直接跳过(`key_ordered_consumer.go:482-518`)。压测结果里会出现假的「存盘丢失」。
  - 旧映射还会把新号路由到错误的库。
- **本机**:docker-compose 不拆,`stress_round19.ps1:171-191`、`currency_crash_window.ps1:175-186` 都不用改。
- **K8s 压测环境**(`tidb-phase1-and-launch-readiness.md` §3.5 说的专用环境):新增 `dev_tools.ps1 -Command k8s-redis-wipe -ConfirmWipeAllRedis`。
  - 按 `Get-RedisTopologyFor` 列出的实例逐个 FLUSHALL,然后逐个检查 `DBSIZE`,不为 0 就以退出码 1 结束。拓扑以线上 ConfigMap 读出的现地址为准(同 `k8s-redis-split-status`),不以本次命令行参数为准:参数漏写一个用途,就会漏清那个实例。
  - `redis-match-cluster` 是集群,要对每个主节点各发一次 FLUSHALL,再逐个核对。
  - 白名单不能只靠本次命令行参数(调用者想清哪个就能写哪个)。infra namespace 必须符合固定的压测命名规则(例如 `^mmorpg-stress-`),规则写死在脚本里,不符合就拒绝。
- **压测快照**:`stress_snap.ps1` 增加各实例的 `INFO` 快照,`stress_summarize.ps1` 增加一张「Redis 实例」表(§6.4)。这会改 §6.1 规定的输出口径,需要负责人点头(§13 第 9 条)。
- **AGENTS.md 本身**由负责人改,本稿不动。建议改法:把「redis-cli FLUSHALL」改成「对部署拓扑里的每个 Redis 实例 FLUSHALL,并核对 DBSIZE=0(K8s 用 dev_tools k8s-redis-wipe)」。
- **对拆分本身的要求**:拆分有没有效果,必须用 L-14 尖峰压测的对比表来说明,拆分前一轮要先存 prev-summary。没有对比表,不能宣称「Redis 不再是瓶颈」。

## 6. 容量、maxmemory 与淘汰策略

本节数字**全部是推算**,没有压测。

### 6.1 负载模型

变量:
- N:在线人数;
- C:每秒建角数;E:每秒进场数;R:每秒读任务数;
- Q:排队人数;P:客户端轮询间隔(秒);
- L:login 实例数,每 zone 2 个副本,即 L = 2Z;
- W:每秒写任务数,约 2N/300。每次存盘最多涉及 2 张子表,存盘周期 300s(`cpp/libs/services/scene/core/system/redis.cpp:121`)。

| 实例 | 主要命令来源 | 推算 ops/s |
|---|---|---|
| redis-queue | 排队 Lookup 每次 ≥4–5 条;放行循环每秒每 zone 一次 ZPOPMIN,每放行一人 3 条;快速通道 1 次 EVAL;限流每个请求 2 个桶 | ≈ 5Q/P + 3 × 放行数/s + 2 × 请求数/s |
| redis-mapping(第 1 批) | go/db 每个任务 2 次 MGET;每次进场 1 次 EVAL;每次角色列表 1 次 MGET;每次建角 2–3 条(db_first 模式再加 WAITAOF);心跳 | ≈ 2W + E + 列表数/s + 3C |
| redis-mapping(第 2 批后) | 再加每个写任务约 7 条控制面命令:锁 SETNX、游标 GET / SET、epoch GET / EVAL、解锁 EVAL | ≈ 9W + E + … |
| redis-cache | 每个任务 1 次大 value 的 SET;每个读任务 LPUSH / EXPIRE / PUBLISH 各一次;PUBLISH 扇出到 L 个 login | ≈ W + 3R,另有推送 R × L 条 |
| redis(核心) | C++ 存盘每玩家每 300s 一次 EVALSHA;进场链路(锁、父键、会话、位置 / epoch 的 Lua、场景计数);离场;战斗与组队;会话续租 | 待压测 |

例:N = 10 万时 W ≈ 667/s。第 1 批 `redis-mapping` 约 1.3k 次 MGET/s;第 2 批后约 6k ops/s。单实例 Redis 处理简单命令的量级是每秒几万到十万,稳态余量很大。**首日真正的风险是尖峰**,也就是 E、C、Q 三项,而不是稳态。

### 6.2 内存模型

| 实例 | 内存主项 | 增长口径 | 推算 |
|---|---|---|---|
| redis-queue | 每个排队玩家:ZSET 成员 + meta HASH + token 键,约 0.5–1KB;每个 IP 桶约 0.2KB | 与在途排队人数、活跃 IP 数成正比,带 TTL | 10 万人排队约 100MB |
| redis-mapping | 每个玩家的 `player:zone`(加上 placement、maprev)约 0.1–0.3KB;第 2 批起,每个存过盘的玩家再加「表数(2)× 2 个游标键」约 0.5KB | **随累计注册 / 存过盘的玩家数增长,无 TTL,不会回落** | 1000 万玩家约 3–8GB |
| redis-cache | 最近一个 TTL 内(K8s 为 1h)写过的玩家 × 子表大小 | 与近 1h 活跃玩家数成正比 | 待压测 |
| redis(核心) | `PlayerAllData` 父键(2026-06 实测每玩家约 21KB / 17 个键,`docs/notes/redis-oom-data-safety-qa-record.md:21`)+ `owner_epoch`(永不删)+ 账号 + `dirty_keys_set` 成员 | **随累计进过场景的玩家数增长**(§2.5 第 1、2 条) | 累计 100 万玩家约 20GB |

**核心内存没有上界,这是比拆分更急的容量问题**:
- 核心一旦设了 maxmemory + noeviction,满了就拒写,全服存盘和登录都会失败。
- 不设 maxmemory,就是今天的状态:最终被 OOMKill。
- 要么先有冷玩家卸载,要么按累计玩家规模配内存(§13 第 5 条)。

### 6.3 maxmemory 与淘汰策略

**所有实例都设 `maxmemory-policy noeviction`,并且显式写出,不依赖 Redis 的缺省值。** 各族被淘汰的后果:

| 族 | 被淘汰的后果 |
|---|---|
| 映射 | 缺键被当成「无记录」,写进错误的库(`docker-compose.yml:228-231`;proposal §3.3) |
| 游标 / applied_epoch | 顺序守卫静默失效(同上) |
| 排序锁 / 重试收据 | 并发写;收据丢失,存盘随之丢失 |
| `PlayerAllData` 父键 | 写回式的最新态丢失,等于回档 |
| 账号目录 | 角色从账号里消失,没有库可以回源 |
| 锁 / 会话 / epoch | 出现双持有者;顶号判断出错 |
| 排队 / 准入 | 玩家丢失排位;gate 令牌超发 |
| 限流桶 | 限流静默失效(friend 的注释 `k8s_deploy.ps1:4062-4064` 说的是同一件事) |
| 子表缓存 | 安全,只是多一次冷读 |
| `task:result` | 读等待 8s 超时,进场失败 |

只有子表缓存能容忍淘汰,但它和 `task:result` 放在同一个 `redis-cache` 里,所以这个实例也用 noeviction。

- **取值**:`maxmemory` = 压测峰值 × 1.5;`limits.memory` = 2 × maxmemory。release 档不给值就拒绝部署;dev 档用很小的缺省值。D1 的 noeviction 自检需要 `CONFIG GET` 可用,所以不能禁用这个命令。
  - `redis-queue` 的峰值要按最坏情况算 `rl:ip:*`:每个出现过的 IP 一个桶,TTL 是重填时间加 1h(`RateLimitConfig.java:71-91`)。trusted-proxies 配错时 IP 可被 X-Forwarded-For 伪造(`ClientIpResolver.java:20`),桶数没有上界。它满了之后,排队开着时新登录全部失败(§7),所以这个实例的内存告警要比别的实例更早。
- **持久化**:全部用 AOF everysec;核心维持现状(`redis.yaml:95-97`)。`redis-mapping` 上的 W1 用 `WAITAOF 1 0`(D1)。要不要改成 `appendfsync always`,按 D1 §11.1 第 5 条,压测后再定。

### 6.4 监控

- **现状**:没有 redis_exporter(`deploy/`、`tools/scripts/` 里 grep 不到)。
- **第 1a 批先不引入新镜像**:
  - `stress_snap.ps1` 增加 `-RedisTargets` 参数。每个快照时刻,对每个实例执行 `kubectl exec … redis-cli INFO stats|memory|cpu|commandstats|persistence`,存成 `t<N>m_redis_<name>.txt`。
  - `stress_summarize.ps1` 出一张表,包含:ops/s、`used_memory`、`used_memory` 占 maxmemory 的比例、OOM 拒绝次数(`errorstat_OOM`)、`rejected_connections`、`aof_delayed_fsync`、各类命令的 calls / usec_per_call。
- **长期**:每个实例挂 redis_exporter sidecar,并配告警:内存超过 maxmemory 的 80%、出现 `errorstat_OOM`、AOF 写错误(§13 第 6 条)。
- **指标 label** 只用实例名(低基数),不用 player_id(AGENTS §9)。

## 7. 错误语义

- **部署期拒绝**:非 0 退出,并给出中文原因。拒绝的情形有:
  - dbctl 没带 mapping;
  - release 档没给 maxmemory;
  - 线上改地址时没给 Confirm(包括例行部署漏带 `-RedisSplit`),或目标实例上没有迁移标记,或目标标记不比源标记新;
  - 启用的用途对应的 Service 不存在;
  - 同一用途在一个 zone 内的两份 ConfigMap 不一致(login 与 gateway、login 与 db 的 cache 句柄);
  - 改核心模板时没给 `-ConfirmRedisCoreRestart`;
  - 切 queue 时有正在进行的排空,或任一 zone 的排队开着。
- **工具期拒绝**:merge_zone、data_consistency_check、route_tool 连到带墓碑的实例(§5.2「误配防线」)。
- **login 启动**:QueueRedis / CacheRedis Ping 失败就 panic,与 RedisClient 同口径(`servicecontext.go:118-120`)。
- **login 运行期**:
  - 排队句柄出错:AssignGate 走既有的错误返回,客户端重试;
  - 排空查询失败:放行(`gatedrain.go:60-81`,不变)。
- **go/db**:
  - CacheRedis 连不上:只记 ERROR,不拒启,与 `svc/servicecontext.go:31-47` 同口径。
  - 写任务的缓存回写失败:任务重试,游标不推进(`key_ordered_consumer.go:836-843`,不变)。
  - `redis-mapping` 上的控制面不可用:行为与今天核心不可用时相同,拿不到锁、读不到游标就重试。
- **各实例满了(noeviction 拒写)时**:
  - `redis-mapping` 拒写:W1 建角失败,返回 Unavailable;login 按 fail-closed 处理并释放名字(`createplayerlogic.go:257-275`)。go/db 游标写失败 → 重试耗尽预算 → 进死信。这会放大 L-04 的问题(§12 第 8 条)。
  - `redis-queue` 拒写或不可用:排队开着时 AssignGate 对新登录一律返回 `StatusError`(fail-closed,§3.2.1);排队关着时不受影响。限流 fail-open,排空过滤放行。
  - 核心拒写:C++ 存盘失败并无限重试(`player_lifecycle.cpp:5401` 的注释),登录失败。与今天不设 maxmemory 时会被 OOMKill 相比,变成了「拒写,但进程活着」。
- **`redis_split`**:退出码 0 / 1 / 2 / 3,见 §5.2。

## 8. 测试清单

| 用例 | 位置 | 外部依赖 | 批 |
|---|---|---|---|
| `TestNodeConfig_QueueRedisAbsentReusesRedisClient` | `go/login/internal/config/queue_redis_conf_test.go`(新) | 无 | 1a |
| `TestNodeConfig_QueueRedisFullBlockLoads` | 同上 | 无 | 1a |
| `TestNodeConfig_QueueRedisPartialBlockRejected`(钉住 go-zero 的语义:段出现时内层字段必填) | 同上 | 无 | 1a |
| `TestRepoLoginYaml_HasNoQueueRedis`(本地 yaml 保持共享) | 同上 | 无 | 1a |
| `TestNewQueueRedis_NilConfigSharesPointer` | `go/login/internal/svc/queue_redis_test.go`(新) | 无(miniredis,login 已依赖) | 1a |
| `TestInitLoginQueue_WritesToQueueRedisOnly`(两个 miniredis:Enqueue 只落在 queue 实例) | 同上 | 无 | 1a |
| `TestCapacityProvider_DrainingReadFromQueueRedis` / `TestGateDrainMonitor_UsesQueueRedis` | 同上 | 无 | 1a |
| 契约:缺省拓扑下所有 ConfigMap 的 Redis 地址都是 `redis.<ns>`,且不渲染 `redis-purpose` | `k8s_deploy_contract.tests.ps1` | 无 | 1a |
| 契约:缺省拓扑的渲染结果与改造前逐字节相同(golden) | 同上 | 无 | 1a |
| 契约:`-RedisSplit queue,mapping` 时,login QueueRedis、gateway、data-service MappingRedis、db Placement.Redis 都指向对应实例;db RedisClient 仍是核心 | 同上 | 无 | 1a |
| 契约:任何拓扑下 C++ `zoneredis` 都是 `redis.<ns>` | 同上 | 无 | 1a |
| 契约:任何拓扑下 scene-manager / player-locator / match / chat / trade / friend 的共享段都是核心;data-service 的 DevRedis / Regions 也是核心 | 同上 | 无 | 1a |
| 契约:dbctl 不带 mapping 时拒绝;release 档启用用途却不给 maxmemory 时拒绝 | 同上 | 无 | 1a |
| 契约:`redis-purpose` 渲染结果含 noeviction、appendonly、maxmemory、limits = 2 × maxmemory、`REDIS_PASSWORD` 环境变量,且没有残留占位 | 同上 | 无 | 1a |
| 契约:release 档的 gateway 写了 `spring.data.redis.password`;dev 档(口令为空)不写,渲染逐字节不变 | 同上 | 无 | 1a |
| 契约:`redis-purpose` 渲染出的对象标签都是 `app: redis-<purpose>`,没有 `app: redis` | 同上 | 无 | 1a |
| 契约:任何拓扑下 auth 的 `spring.data.redis.host` 是 `redis.<ns>` | 同上 | 无 | 1a |
| 契约:`:839` 的断言抽成函数,对 `queue,mapping`、`mapping,dbctl`、`mapping,dbctl,cache` 三种渲染各调一次 | 同上 | 无 | 1a / 2 |
| `Assert-RedisTopologyChangeIsSafe`(注入假的 `-ReadLiveConfigMap` / `-ReadMarker` / `-ServiceExists` / `-ScanCount`):地址要变但没给 Confirm → 拒;漏带 `-RedisSplit`(线上已拆、本次为空)→ 拒且报错写明现拓扑;mapping 目标无标记 → 拒;目标标记不比源标记新 → 拒;目标 Service 不存在 → 拒;queue 切换时有 draining 或某 zone `Queue.Enabled=true` → 拒;login 与 gateway 的 queue 地址不一致 → 拒;一切齐全 → 放行;ConfigMap 不存在 → 不查地址变化 | 同上 | 无 | 1a |
| `k8s_zone_rollback.ps1`:给了 `-RedisSplit` 时 Step 6 的 zone-up 参数里带上它;拓扑闸在停服之前执行,不一致时在 Step 1 之前拒绝 | `k8s_zone_rollback_gate_router_mode.tests.ps1` 同款写法(新文件 `k8s_zone_rollback_redis_split.tests.ps1`) | 无 | 1a |
| 守卫新增的四个核心 pattern 各自命中都拒绝;只有 `PlayerAllData:*` 的 zone 缓存放行 | `k8s_zone_rollback_flush_guard.tests.ps1` | 无 | 1a |
| 契约:不给 `-RedisCoreMaxMemory` 时 `redis.yaml` 渲染逐字节不变;给了却没给 Confirm 时拒绝 | 同上 | 无 | 1a |
| 契约:既有的 `:839` 用例在所有拓扑下都通过 | 同上 | 无 | 1a / 2 |
| `QueueRedis 段存在时 exec 目标是 redis-queue` / `缺省回落 Node.RedisClient` / `QueueRedis.DB 非法时拒绝` | `k8s_gate_drain.tests.ps1` | 无 | 1a |
| `核心族命中时拒绝 FLUSHDB` / `只有排队族的实例放行` | `k8s_zone_rollback_flush_guard.tests.ps1` | 无 | 1a |
| `k8s-redis-split-status`:queue / mapping / cache 在各 zone 地址不一致时退出码为 1;任一 rollout 未完成时退出码为 1;dbctl 各 zone 不一致只打印、退出码 0;zone 列表走 `Resolve-ZonesConfigPath`(yaml 也能读)。`k8s-redis-wipe` 没有确认开关或 namespace 不符合压测命名规则时拒绝 | `tools/scripts/tests/dev_tools_redis_split.tests.ps1`(新) | 无 | 1a |
| `dev_tools.ps1` 把 `-RedisSplit` 等参数原样转给 `k8s-zone-up` / `k8s-all-up` / `k8s-zone-rollback`(DryRun 输出里能看到) | 同上 | 无 | 1a |
| `TestFamilies_NoOverlapAcrossPurposes`、`TestFamilies_PatternsCoverRollbackGuard`、`TestFamilies_ConsumerLockHasLockerPrefix`(必须为空的扩分区锁 pattern 是 `distributed:lock:kafka:consumer:lock:*`。redis_split 是独立 module,引用不了 go/db 的 internal 包,所以照 `tools/merge_zone/relocate.go:367-372` 的做法镜像前缀常量,注释写明改一处必须同步另一处) | `tools/redis_split/families_test.go` | 无 | 1b |
| `TestTopics_ExactMatchOnly`:`-topics db_task_zone_1` 不命中 `db_task_zone_10`、`db_task_zone_1_g2` 的任何键;非法 topic 名(含 `*`、空串、不匹配正则)退出码 2 | `families_test.go` | 无 | 1b |
| `TestPlan_MustBeEmptyFamilyNonEmptyFails`、`TestPlan_CacheTargetMustBeEmpty`、`TestCopy_RefusesExistingTargetKeys`、`TestCopy_PreservesTTLAndTypes`、`TestCopy_SourceRunIDChangedExits3`、`TestScan_SinglePassClassifiesAllFamilies` | `copy_test.go` | 无(接口 + 假实现) | 1b |
| `TestDelta_FillsMissingOnly`、`TestDelta_ConflictStopsWithoutOverwrite` | `delta_test.go` | 无 | 1b |
| `TestRestore_OverwritesStaleTargetValues`(目标上有更小的游标,restore 后等于源)、`TestRestore_RemovesTargetOnlyKeys`、`TestRestore_RequiresConfirmOverwrite`、`TestRestore_TopicFilterLeavesOtherZonesUntouched` | `restore_test.go` | 无 | 1b |
| `TestVerify_ValueMismatch`、`TestVerify_ExtraKeyInTarget`、`TestMark_RequiresVerifyPass`、`TestMark_WritesTombstoneOnSourceAndClearsTargetTombstone`、`TestUnmark_RemovesMarker`、`TestPurge_RequiresMarkerSoakAndConfirm`、`TestPurge_IncludesRouteReadyMarker`、`TestPasswordOnlyFromEnv` | `verify_test.go`、`marker_test.go` | 无 | 1b |
| `TestMappingHandleRefusesTombstone` | `tools/merge_zone`、`tools/data_consistency_check`、`go/data_service/cmd/route_tool`(各一条,假实现) | 无 | 1b |
| `TestIT_CopyVerifyMarkRoundTrip`、`TestIT_DeltaAfterLateWrite`、`TestIT_RestoreRoundTrip`、`TestIT_PurgeDeletesOnlyPurposeFamilies` | `integration_test.go`(`integration` tag;用 DB 14 / 15 当源和目标,只 FLUSHDB 这两个库) | **需要本机 Redis** | 1b |
| `TestCacheRedisNilSharesRedisClient`、`TestConfig_CacheRedisOptional` | `go/db/internal/svc`、`internal/config` | 无 | 2 |
| `TestProcessDBTask_NormalPathPublishesToCacheRedis`、`TestProcessDBTask_OfflineExpandPublishesToCacheRedis`、`TestTryLockAndProcess_PublishesToCacheRedis`、`TestFailReadTask_PublishesToCacheRedis`、`TestCursorEpochLockStayOnRedisClient`、`TestNewKeyOrderedKafkaConsumer_WiresThreeHandles`、`TestNewKeyOrderedKafkaConsumer_NilCacheRedisRejected` | `go/db/internal/kafka/cache_redis_test.go`(新,两个 miniredis);`placement_route_test.go:891-895` 同步改成五参数 | 无 | 2 |
| `TestDbCtlRedisDefaultsToNotice`、`TestPreflight_RetryQueuesReadFromDbCtl`、`TestRelocate_OrderingLocksFromDbCtl`、`TestDbCtlHandleRefusesTombstone` | `tools/merge_zone`(假实现) | 无 | 2 |
| 契约:`-RedisSplit mapping,dbctl` 时 db 的 RedisClient = redis-mapping、CacheRedis = redis,且不写 Placement 段 | `k8s_deploy_contract.tests.ps1` | 无 | 2 |
| `TestEnsurePlayerAllData_ParentOnCoreSubOnCache`、`TestTaskResultDispatcher_SubscribesCacheRedis` | `go/login/internal/logic/pkg/dataloader`、`internal/dispatcher` | 无 | 3 |
| `TestInvalidatePlayerCaches_SplitsParentAndSubKeys` | `tools/merge_zone` | 无 | 3 |
| kind 演练:新 namespace 下拆分 + robot login-test + 排空 + 杀 Pod | §10 | **需要 kind** | 1c / 2 / 3 |

## 9. 归属目录在 Redis 侧会不会成瓶颈(回应负责人追问)

- **读写模型**(§6.1):
  - 写:每个玩家只在建角时写一次,另有合服运维时的批量改写。
  - 读:go/db 每个任务 2 次 MGET;每次进场 1 次 Lua;每次角色列表 1 次 MGET。
  - 全都是 O(1) 的小键,单实例的余量远大于首日推算值。这一点仍要用 L-14 压测证实。
- **先到瓶颈的不是映射本身**,而是第 2 批并进来的存盘控制面:每个写任务约 7 条命令。压测如果显示 `redis-mapping` 的 CPU 吃紧,第一步是把控制面拆到第四个实例,只改配置(§13 第 2 条)。
- **再往后**:
  - 映射不能靠只读副本扩容,因为 go/db 必须读主节点。
  - 要集群化,得把 `player:zone`、`placement`、`maprev` 改成同一个 hash tag 的 `player:{id}:…`,并把围栏复核从 Redis 发布脚本挪到库里。D1 的 db_first 提交点已经在库里(proposal §4),所以这是 D1 P2 之后的事,本稿不做。
- **全球多地域**:目录留在单一写入地域(proposal `:783-786`;`tidb-phase1-and-launch-readiness.md` §8)。
- **与库侧的关系**:D1 §12 已经论证,全局库上的映射写不是一种新的瓶颈。Redis 侧拆出 `redis-mapping` 之后,目录的热路径不再与会话、存盘、排队抢同一个 CPU。
- **本节只回答 Redis 侧**。负责人问的「归属全服库」如果指全局库里的 `player_route` 表(D1 db_first 的提交点),那是库侧的问题,答案在 `player-route-mapping-durability-proposal.md` §12(吞吐上限约为「每 zone 分片数 ÷ 事务时长」、TiDB 上的尾部 Region 与围栏整表热点)和 `tidb-phase1-and-launch-readiness.md` §5,本稿不重复。

## 10. 交给 Codex 的验证命令与通过标准

工作目录都是仓库根。C++ 不涉及,不需要 MSBuild。任何一步失败都保留输出摘要并停下,不要连续自动重试。

**第 1a 批**

```
cd go/login && gofmt -l ./internal
cd go/login && go build ./... && go vet ./... && go test -count=1 ./internal/config/... ./internal/svc/... ./internal/logic/pkg/loginqueue/...
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_gate_drain.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_redis_split.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_redis_split.tests.ps1
python tools/scripts/gen_docs_index.py --check
```

通过标准:
- gofmt 没有输出;
- 所有用例 PASS,或 fail=0;
- 契约测试的新用例全部出现在输出里;
- `gen_docs_index --check` 退出码为 0。

如果 Java 只改了注释,`mvn` 可以不跑;改了 Java 代码就要在 `java/gateway_node` 跑 `mvn -q test`。

**第 1b 批**

```
cd tools/redis_split && gofmt -l . && go build ./... && go vet ./... && go test -count=1 ./...
cd tools/merge_zone && gofmt -l . && go build ./... && go vet ./... && go test -count=1 ./...
cd tools/data_consistency_check && gofmt -l . && go build ./... && go vet ./... && go test -count=1 ./...
cd go/data_service && gofmt -l ./cmd/route_tool ./internal/routetool && go build ./... && go vet ./cmd/route_tool/... ./internal/routetool/... && go test -count=1 ./cmd/route_tool/... ./internal/routetool/...
# 需要本机 Redis(docker compose -f deploy/docker-compose.yml up -d redis)。PowerShell:
$env:REDIS_SPLIT_IT_ADDR='127.0.0.1:6379'; cd tools/redis_split; go test -tags=integration -count=1 ./...
```

通过标准:全部 PASS;集成测试只动 DB 14 / 15(跑完后用 `redis-cli -n 0 DBSIZE` 核对,DB 0 不变)。墓碑检查只读一个 `EXISTS`,不会改动既有 merge_zone 用例的行为,既有用例必须全部原样通过。

**kind 演练(第 1c 批之前,有外部依赖)**

1. 在新 namespace 执行 `dev_tools.ps1 -Command infra-up -RedisSplit queue,mapping -WaitReady`。期望:
   - `redis-queue-0` 和 `redis-mapping-0` 都 Ready;
   - `kubectl exec redis-mapping-0 -- redis-cli CONFIG GET maxmemory-policy` 返回 `noeviction`。
2. 两个 zone 执行 `zone-up`。期望:
   - login 横幅里有 `queue: redis-queue…`;
   - go/db 横幅的落点行是 `redis-mapping…`;
   - robot login-test 通过;
   - 用 `k8s_gate_drain.ps1` 标记一台 gate 排空,login 日志里按排空剔除了它;
   - 核心上 `SCAN MATCH player:zone:*` 为 0,`redis-mapping` 上的映射数随建角增长。
3. 演练一次 §5.4 路径 A 的完整流程:先在共享拓扑下建出一批角色,再迁移。期望:
   - verify 退出码为 0;
   - 冻结期间用 killswitch 关掉 RegisterPlayerZone,并人为在核心上写一个映射键,之后 delta 能把它补进 `redis-mapping`;
   - 部署闸在没有标记时拒绝;
   - 记录 copy / verify 的耗时,port-forward 与集群内 Job 各一次(§5.2「执行位置」),据此修正 §5.4 的冻结时长;
   - 切换后不带 `-mapping-redis-addr` 跑一次 merge_zone 只读审计,被墓碑拒绝。
4. 演练一次 §5.8 的 mapping 回退:解冻后再建一批角色,并在 `redis-mapping` 上人为改一个已有玩家的 `player:zone` 值;回退用 `restore`。期望:核心上新建角色齐全、被改的值以 `redis-mapping` 为准;之后不重新拷贝就尝试再次正向切换,部署闸以「目标标记不比源标记新」拒绝。
5. 不带 `-RedisSplit` 对已拆分的 zone 跑一次 `zone-up`(非 DryRun):部署闸拒绝,报错写明现拓扑;带 `-RedisSplit` 跑 `k8s-zone-rollback` 的 dry-run,输出里 Step 6 带着它。
6. 默认档回归:不带 `-RedisSplit` 跑 `k8s_deploy.ps1 -DryRun`,dev 档产物与改造前逐字节相同;release 档只多出 gateway 的 `password` 一行。

**第 2 批**

```
cd go/db && gofmt -l ./internal && go build ./... && go vet ./... && go test -count=1 ./internal/kafka/... ./internal/config/... ./internal/svc/...
cd tools/merge_zone && go build ./... && go vet ./... && go test -count=1 ./...
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1
```

kind 上两个 zone 按 §5.5 逐个 zone 迁移。zone 号要选成一个是另一个的前缀(例如 1 和 10),专门验证 `-topics` 不串 zone。期望:
- robot 的存盘没有中断(Kafka 缓冲之后能追平);记录每次缩容 / 扩回时其它 zone 的 lag 尖峰(L-09 的全组 rebalance);
- 游标在 `redis-mapping` 上增长;迁移 zone 1 之后,zone 10 的游标在核心上照常增长、在 `redis-mapping` 上一条都没有;
- 对 zone 1 演练一次 `restore` 回退:回退前在 `redis-mapping` 上让 zone 1 的游标继续前进,回退后核心上的值与之相等,不是切换前的旧值;
- `go test ./internal/kafka/...` 里 retry_ownership 的既有用例仍然 PASS。

**第 3 批**:login 与 go/db 的单测、merge_zone 单测、契约测试,另在 kind 上全服一拍切换一次。

## 11. 与现有设计的偏离与代价

1. **任务的四类变成三类加核心**:「会话与锁」「PlayerAllData 缓存」不能按字面拆(§3.3)。代价:核心仍是最大的单点。
2. **no-single-node §5 原本在「哨兵」与「按用途拆库后分别集群化」之间待定**。本稿只拆,不集群化,也不做哨兵。代价:每个实例仍是单点,可用性没有提高,只是缩小了爆炸半径。
3. **D1 §5.2 的论证在第 1 批与第 2 批之间暂时不成立**:映射已经在 `redis-mapping`,游标还在核心。
   - 只丢 `redis-mapping` 时,D1 第 3 批之前 go/db 会走 legacy 分支,按本 zone 选库(`placement_route.go:171-172`)。
   - 今天所有环境都是 `PinOnCreate=false`,也从没执行过 pin / relocate(`docs/design/player-storage-placement.md:5`),所以 legacy 分支的结果就是对的。
   - D1 第 3 批读方开关之后,行为变成延后写。
   - 建议第 1 批和第 2 批之间不要跨过「打开 PinOnCreate」或「执行 pin / relocate」。
4. **merge_zone 的句柄从 5 组变成 6 组**(加 `-dbctl`),dev_tools 的参数面随之变大。
5. **同址契约从隐式变显式**:契约测试 `:839` 不改,但「go/db 读落点的 Redis 与 MappingRedis 同址」从「都在核心,隐式同址」变成「都显式指向 redis-mapping」。
6. **CZ-2 / D12 不变**:核心仍是物理共享的单实例。
7. **Java gateway 改指 `redis-queue`**:`RateLimitConfig.java:84` 注释里「与 login 的 token 是同一实例」要更正。
8. **新增一个 Go module `tools/redis_split`**,不引入 miniredis。单测靠接口,真 Redis 走 integration tag,与 merge_zone 同口径。
9. **第 1a 批有两处缺省即生效的改动**(§1 第 4 条):release 档 gateway 补口令、回档守卫补核心族。代价:release 档的 gateway ConfigMap 多一行;守卫今天看不到变化。login 超时参数的修复因为会改核心 client 的行为,刻意不纳入。
10. **`-RedisSplit` 成了每次部署都要带的参数**。代价:例行部署、回档、发布入口都要转发;漏带由部署闸拦下而不是静默回退,但会让一次例行部署失败。备选是把拓扑存进 infra namespace 的 ConfigMap、zone-up 缺省读它;那样 DryRun 读不到集群,与 Kafka 拓扑的现有做法也不一致,所以本稿不采用,列入 §13 第 16 条。

## 12. 风险

1. **配错地址就会让映射静默写错库**。防线:迁移标记(且必须比源上的新)、契约测试、`k8s-redis-split-status`(含 rollout 完成)、墓碑(工具连到旧实例即拒绝)、purge 让配错的组件大面积失败而不是静默出错。
   - 残余:purge 之前的浸泡期里,绕过部署闸手工改回 ConfigMap 的服务读到的是冻结时的旧映射,只缺冻结之后新建的号。§13 第 15 条的服务启动期墓碑检查能关掉这个窗口;D1 就绪标记上线后也能关掉。
2. **killswitch 是 fail-open 的**:由 delta 兜住;delta 发现冲突就交给人处理。
3. **核心内存没有上界**(§2.5 第 1、2 条)。这与拆分无关,但决定了 maxmemory 能不能设:设了 noeviction + maxmemory 之后,满了就拒写,全服存盘和登录都会失败。
4. **C++ 不发 AUTH**:release 档核心一开 requirepass,C++ 全线 NOAUTH。本稿的新实例都没有 C++ 客户端,可以开密码;核心怎么处理待拍板。
5. **实例变多,运维面变大**:PVC、备份都要多管几份。Redis 今天没有任何备份,AOF 只在 PVC 上。映射在 D1 之后有库里的真源,控制面在库侧没有备份。
6. **第 2 批逐 zone 停 go/db**:存盘暂停期间,新号首次进场冷读会失败(8s 超时)。而且在 L-09 之前,每次缩容与扩回都会让**全服**存盘停几秒(共用消费组的 rebalance),Z 个 zone 一共 2Z 次。
7. **迁移期间必须禁用 merge_zone 和 relocate,第 2 批期间还要全服禁止在线扩分区**,目前只靠运维纪律和手册约束;墓碑只能拦住「连错实例」,拦不住「在窗口里运行」。
8. **`redis-mapping` 满了时**,go/db 写游标失败 → 重试耗尽预算 → 进入没有消费者的死信(L-04)。拆分把这个风险集中到了 `redis-mapping` 的容量上,必须配告警。
9. **每个新实例都是单点**:
   - `redis-queue` 挂掉:排队关着时只影响排空过滤与限流(都放行);排队开着时新登录全部失败(fail-closed,§3.2.1),已在线玩家不受影响。
   - `redis-mapping` 挂掉:建角、进场(要读 home)、存盘全部停止,影响面与今天核心挂掉相同,只是故障源多了一个。
   - 所以 `redis-mapping` 是 HA 的第一优先级。
10. **WAITAOF 在 `redis-mapping` 上的延迟没测过**(D1 L-12)。
11. **回退不能只补缺**。旧实例上的旧值会被当成权威:映射是旧归属,控制面是更小的游标与 epoch(重放已落库的任务),子表缓存比库旧(拼父键等于回档)。所以映射与控制面回退一律 `restore`,缓存回退先清核心上的子表缓存(§5.8)。
12. **`-RedisSplit` 漏带**:由部署闸拦下,不会静默回退,但会让例行部署或回档失败。回档脚本在停服前真查一次拓扑(§4.2),把失败挪到停服之前。

## 13. 待负责人拍板

1. 是否采纳「核心 + 2 个新实例 + 1 个可选实例,分 3 批」,以及「会话与锁 / PlayerAllData 父键不拆」的结论。
2. 控制面是并入 `redis-mapping`(本稿推荐),还是单独开第四个实例。
3. 第 3 批(`redis-cache`)做不做、什么时候做;是否先做 `task:result` 按 login 实例分频道。
4. 各实例 maxmemory 与 limits 的初值(等 L-14);核心是否在第 1c 批的维护窗口里显式化(需要一次全服重启)。
5. 核心内存无上界怎么处理:另写一份冷玩家卸载设计,还是按累计玩家规模配内存。
6. Redis 监控:先做 INFO 快照(不引入新镜像),还是直接上 redis_exporter sidecar。
7. C++ AUTH:给 `RedisManager` 加 AUTH,还是核心不开 requirepass、改用 NetworkPolicy 限制访问。
8. AGENTS §6.2 中 Redis 清库口径的改写(由负责人改 AGENTS.md)。
9. `stress_summarize` 增加 Redis 表(会改 §6.1 规定的输出口径)。
10. 拆分与 L-02 的先后。建议先拆 queue,再打开排队与限流。
11. 映射迁移走路径 A(冻结拷贝,今天就能做),还是等 D1 第 1b、3 批到位后走路径 B。
12. purge 前的浸泡时长(缺省 24h),以及由谁执行。
13. 新实例是否也用 redis-ha 形态(`redis-mapping` 优先),以及补上 `-RedisReplicas` 的 apply 路径(`tidb-phase1-and-launch-readiness.md` §9.5)。
14. `deploy/k8s/AGENTS.md:24`、`:82` 对 Redis 拓扑的描述要随落码更新。它是 AGENTS 文件,改不改由负责人确认。
15. data_service 与 go/db 启动时是否检查自己映射句柄上的墓碑(命中拒启,§5.2「误配防线」)。做了能兜住手工改 ConfigMap 的绕过,代价是第 1 批不再是纯配置改动。
16. 拓扑是继续用「每次部署带 `-RedisSplit`」(本稿),还是存进 infra namespace 的 ConfigMap、由 zone-up 缺省读取(§11 第 10 条)。
17. 第 2 批是否等 L-09(消费组按 zone 命名)做完再开始(§5.5)。
18. login Redis 超时参数没传进 client(§2.5 第 8 条)单独修不修;修的话核心 client 的 DialTimeout 从 5s 变 3s,要单独评审。

## 14. Java 版(AGENTS §12)

- **现状**:
  - Java 版只有一个 `game.redis.*`,用 Redisson,缺省 DB 12(`D:\luyuan\xuanming-server-mmo\game-discovery\src\main\java\com\game\discovery\RedisProperties.java`)。
  - 排队与限流在 game-gateway 里(`queue/LoginQueue.java`、`queue/QueueDispatcherRunner.java`、`ratelimit/RedisRateLimitStore.java`)。
  - 账号与角色在 MySQL(`PARITY.md:41`)。
  - 没有 K8s 部署,只有 compose,单个 Redis(`deploy/compose/infra.yaml:59-64`)。
- **判断**:本拆分是服务端内部的部署形态,不涉及客户端契约(AGENTS §12「两版共享的只有客户端契约」)。Java 版状态 = **待做**。
  - 原因 1:Java 还没有生产部署形态。
  - 原因 2:它的键族与本仓不同(账号在库里),拆分要按 Java 自己的键重新盘点。
- **建议做法**:在 game-discovery 的 `RedisAutoConfiguration` 里按用途提供多个 RedissonClient bean,例如 `game.redis.queue.*`,缺省回落到 `game.redis.*`;先拆排队与限流。
- **PARITY.md 登记**:第 1 批落码时登记一行「共享 Redis 按用途拆实例」:mmorpg commit ↔ Java「待做」+ 上述原因。

## 15. 文件清单

**第 1a 批**(约 22 个文件)

新增:

| 文件 | 要点 |
|---|---|
| `deploy/k8s/manifests/infra/redis-purpose.yaml` | 模板,占位见 §4.3 |
| `go/login/internal/config/queue_redis_conf_test.go` | §8 |
| `go/login/internal/svc/queue_redis_test.go` | §8 |
| `tools/scripts/tests/dev_tools_redis_split.tests.ps1` | §8 |
| `tools/scripts/tests/k8s_zone_rollback_redis_split.tests.ps1` | §8 |

修改:

| 文件 | 要点 |
|---|---|
| `go/login/internal/config/config.go` | NodeConfig 加 `QueueRedis` |
| `go/login/internal/svc/servicecontext.go` | `newRedisClient`、`newQueueRedis`、`QueueRedis`,四个注入点;client 选项与今天逐字相同 |
| `go/login/login.go` | 启动横幅补 `queue:` 一行(`:466` 之后) |
| `go/login/etc/login.yaml` | 只加注释:不写 QueueRedis = 共享 |
| `tools/scripts/k8s_deploy.ps1` | 新参数、`Get-RedisTopologyFor`、`Assert-RedisTopologyChangeIsSafe`、渲染 redis-purpose、各 ConfigMap 取拓扑、gateway 补密码、核心占位 |
| `deploy/k8s/manifests/infra/redis.yaml` | 加 `__REDIS_CORE_ARGS__`、`__REDIS_CORE_LIMITS__`;缺省渲染逐字节不变 |
| `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | §8 |
| `tools/scripts/k8s_gate_drain.ps1`、`tests/k8s_gate_drain.tests.ps1` | 读 QueueRedis |
| `tools/scripts/k8s_zone_rollback.ps1`、`tests/k8s_zone_rollback_flush_guard.tests.ps1` | 守卫补四个核心 pattern(不含 `PlayerAllData:*`);新增 `-RedisSplit` 并转发给 Step 6;停服前真跑拓扑闸 |
| `tools/scripts/dev_tools.ps1` | `k8s-redis-split-status`、`k8s-redis-wipe`;转发 `-RedisSplit` 等参数 |
| `tools/scripts/stress_snap.ps1` | `-RedisTargets` |
| `java/gateway_node/src/main/java/com/game/gateway/ratelimit/RateLimitConfig.java` | 只改注释 |

文档:本文状态更新;`deploy/k8s/README.md`;`docs/design/no-single-node-horizontal-scaling-20261001.md` §5 状态改为指向本文;`docs/design/tidb-phase1-and-launch-readiness.md` L-06 行加注。

**第 1b 批**:

| 文件 | 要点 |
|---|---|
| `tools/redis_split/{go.mod, go.sum, main.go, families.go, scan.go, copy.go, delta.go, restore.go, verify.go, marker.go, purge.go}` | §5.2(`marker.go` 含 `mark` / `unmark` / 墓碑) |
| `tools/redis_split/{families,copy,delta,restore,verify,marker}_test.go`、`integration_test.go` | §8 |
| 集群内一次性 Job 清单(新,位置随 go-svc 清单目录约定) | §5.2「执行位置」 |
| `tools/merge_zone`(新文件放墓碑检查,`main.go` 接线)+ 测试;`tools/data_consistency_check/main.go` + 新测试;`go/data_service/internal/routetool/redisread.go` + 测试 | 映射句柄的墓碑检查(§5.2「误配防线」) |
| `docs/ops/redis-split-runbook.md` | §5 的运维步骤 |
| `docs/README.md` | 登记 runbook;文件名命中 `*runbook*` 规则,会自动归入「运维手册」 |

**第 2 批**:

| 文件 | 要点 |
|---|---|
| `go/db/internal/config/config.go`、`config_test.go` | `CacheRedis` |
| `go/db/internal/svc/servicecontext.go` | `CacheRedis` 句柄 |
| `go/db/internal/kafka/key_ordered_consumer.go`、`placement_route.go`、`placement_route_test.go`(构造函数改五参数)、`cache_redis_test.go`(新) | `cacheClient`;`processTaskWithoutLock` 的三个调用点 |
| `go/db/db.go` | 构造参数、横幅 |
| `tools/merge_zone/{main.go, preflight.go, audit_resources.go, audit_checks.go, merge_run.go, relocate_run.go, unmerge.go}` + 测试 | `-dbctl-redis-*` |
| `tools/scripts/dev_tools.ps1`、`tests/dev_tools_merge_zone_contract.tests.ps1` | 转发 `-MergeDbCtlRedis*` |
| `tools/scripts/k8s_deploy.ps1`、契约测试 | dbctl 渲染 |
| `docs/ops/merge-zone-runbook.md`、`docs/design/player-storage-placement.md` §6.2、`docs/design/player-route-mapping-durability-proposal.md` §3.3 | 加注 |

**第 3 批**:

| 文件 | 要点 |
|---|---|
| `go/login/internal/config/config.go`、`internal/svc/servicecontext.go`、`internal/logic/pkg/dataloader/{ensure_player_all_data_async.go, sync_loader.go}`、`internal/logic/clientplayerlogin/entergamelogic.go` 的调用点 + 测试 | `CacheRedis`、父键与子表分句柄 |
| `tools/merge_zone/{player_rows.go, main.go, merge_run.go, unmerge.go}` + 测试 | 缓存失效拆分 |
| go/db `cmd/verifier`、`cmd/data_stress`;robot `data_stress.go`;data_service `cmd/debug_fetch` | 参数或 yaml |
| `tools/scripts/k8s_deploy.ps1`、契约测试 | cache 渲染 |

**改动面**:第 1a 批约 22 个文件,第 1b 批约 20 个文件,合计超过 AGENTS §10.2 的 30 个文件门槛。落码前需要负责人授权;授权后也按 1a、1b 严格分两次提交,每次单独由 Codex 验证。
