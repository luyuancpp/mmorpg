<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

## items
  - [0]
    **no**: 1
    **title**: equal 的缓存处理（§14 第 12 条，CPP-3）
    **context**: 同一玩家、同一版本号 (E, s) 的写第二次到达 MySQL 时，库会判「已有同版本」，即 equal。设计默认把它当成同一份内容的重放，于是再补发一次子表缓存。但同一个版本号理论上可能来自两个不同的 scene 进程（dev 旁路，或同一 node_id 的两代进程），两边内容不同，这时补发会让缓存和 MySQL 对不上。要定的是：遇到 equal 时缓存怎么处理。
    ## options
      - [0]
        方案 1（v0.3 默认）：照常补发缓存；跨进程撞号时缓存可能与 MySQL 不同，靠 cmd/verifier 事后核对
      - [1]
        方案 2：在令牌脚本里删掉子表缓存键，由读路径从 MySQL 重建；每次 equal 多一次冷读
      - [2]
        方案 3（在途实现的做法）：equal 时再读一次库里的整行比对内容。内容相同就按方案 1 补发；内容不同判为「同版本冲突」，不发布缓存和资产证据，进死信（独立 reason）并发 critical 告警
    **recommendation**: 方案 3
    **rationale**: 1. 同一版本、内容不同，标准做法是当冲突报错（类似幂等键被复用但参数不同时直接拒绝），既不能静默接受，也不能静默删掉。

2. 在途资产通道已经把这一点定为硬约束，方案 1、2 都满足不了它：
- 04-asset-channel.md 文末 2026-10-15 一段（未提交）写明「同版本不同载荷必须失败」；
- go/shared/assetop/sql_commit.go:18-41（未跟踪的新文件）的证据脚本，同版本不同内容时返回 -1；
- 在途的 publishDBWriteResult（key_ordered_consumer.go:961-970，未提交）在写缓存之前按 (E, s) 发布资产 SQL 提交证据。
撞号时方案 1 会发布与 SQL 不符的资产证据；方案 2 只删缓存，管不到证据。资产 outbox 照这份证据终结，就会错账。

3. 代价很小。equal 只在重试或重投到达 SQL 时出现，大部分重放已被游标挡住，多一次按主键读可以忽略。

4. 实现上要注意三点：
- SaveConditional 被拒时只返回围栏值 Stored，不返回整行（proto2mysql feat/conditional-save 分支的 ConditionalSaveResult），所以要另读一次行；
- 比对应复用 proto2mysql 判 SaveUnchanged 时的逐列口径。在途 player_row_fence.go:45（未跟踪）用的是 proto.Equal，列类型往返可能丢精度（比如 DATETIME 的小数秒），有误判冲突的可能（未核实）；
- 冲突要用独立的死信 reason。在途实现只返回普通 error，按 other 耗完预算后以 budget_exhausted 进死信，事后看不出原因。
    **blocking**: go/db 结局表（§4.4）、写路径（§6.2）、资产 SQL 提交证据的发布条件，以及 §11 第 2 步的 equal 用例；B 批（go/db）无法定稿。
  - [1]
    **no**: 2
    **title**: Observe 超上界的处置与容差（§14 第 13 条，CPP-2）
    **context**: C++ 给每次存盘发一个递增序号 save_seq。设计让整个进程共用一个计数器，加载玩家时用存档里的旧序号把它抬高（Observe）。某名玩家存档里的序号如果大得离谱（坏数据、导入、时钟错乱的主机写出来的），会把整个进程的计数器抬上去，牵连所有玩家，所以设了上界：超过「当前时间 + 容差」的值不采纳。要定的是：超界的那名玩家怎么处理，容差取多少。
    ## options
      - [0]
        方案 1：不采纳。该玩家在本 epoch 内的存盘全部被判 older_seq 丢弃，靠告警和 §9.1 抬计数器处置
      - [1]
        方案 2：超界值只作为该玩家实体上的个人下限，发号取 max(全局 Next, 个人下限 + 1)。不牵连别人，也不丢盘
      - [2]
        容差：设计建议「分钟级」，具体值待定
    **recommendation**: 方案 2；容差取 60 秒，写进 scene 配置；同时加 scene 主机间时钟偏差监控，告警阈值在 1 秒级。
    **rationale**: 1. 方案 1 会把坏数据变成这名玩家确定的丢盘：older_seq 被 ack 丢弃，C++ 看不到，违背 F-3「不静默丢盘」。方案 2 不丢数据，代价只是多一个小字段。

2. 容差为什么取小：容差只决定要不要抬高整个进程的计数器。方案 2 下超界不丢数据，所以容差越小越安全；容差越大，一台时钟超前的主机能把进程计数器抬得越高，越接近 §7.4 条件 4 的风险。60 秒远大于正常 NTP 偏差（毫秒级），也是「分钟级」的下限。

3. 与在途实现冲突：
- 在途 C++ 没有进程级发号器，而是每玩家各自计数：player_ownership_comp.h:104-105 的 lastSaveSeq；player_lifecycle.cpp:4132 播种取 max，:4376 发号 ++lastSaveSeq（都未提交）；
- 没有墙钟下限，也没有上界；计数饱和时直接拒绝存盘（:4371-4374 返回 kRejected），这名玩家在本进程里从此存不了盘；
- 这正是 v0.3 §0.1 第 1 行和 §15 第 2 行否掉的「每玩家组件」形状：父键不是已发出序号的上界，进程崩溃后同 node_id 的新进程又不铸 epoch 时，序号会被复用或倒退（见第 9 项）。

4. 所以负责人要先定以哪一版为准：v0.3 的进程级发号器，还是在途的每玩家计数。如果采纳每玩家计数，至少要补上墙钟下限 max(last + 1, nowMs << 10)，相当于每玩家一个混合逻辑时钟。这样跨玩家污染不再存在，上界只剩防坏数据的作用。
    **blocking**: C++ 发号器 save_seq.h、§7.5 的单测（SaveSeq.ObserveRejectsBeyondBound / PersonalFloorBeyondBound）、scene 配置项；C 批（C++）无法定稿。
  - [2]
    **no**: 3
    **title**: 失租是否豁免重试预算（§14 第 14 条）
    **context**: go/db 写一笔存盘之前要拿 Redis 上的排序锁（租约）。如果中途丢了锁（Redis 抖动、进程卡顿），这笔写会延后重试；重试预算是 3 次，用完进死信。要定的是：「丢锁」这种失败算不算进这 3 次。
    ## options
      - [0]
        不豁免（v0.3 默认）：照常耗预算，连续 3 次失租后以 budget_exhausted 进死信，载荷保留在死信里
      - [1]
        豁免：在 retryBudgetExempt 里加一个分支，失租可以无限重试，靠 db_task_deferred_total{reason="lease_lost"} 观察
    **recommendation**: 不豁免（维持 v0.3），但要订正理由；以后和 L-04 §4.3 的「Redis 侧环境故障」整族一起统一调整，不单独给失租开例外。
    **rationale**: 1. 和 L-04 现在的口径一致：
- L-04 只对存储类失败豁免（placement_route.go:386-397 的 retryBudgetExempt，HEAD）；
- Redis 侧的环境故障（游标读写、epoch 守卫读、缓存发布失败等）目前都耗预算，L-04 §4.3 明确留到「落点 Redis 独立部署时再做」；
- 失租本质上属于 Redis 侧环境故障，单给它开豁免，等于造出第二套规则。

2. 有界：失租反复出现说明 Redis 或进程有系统性问题。无界重试会把整行载荷持续堆在 noeviction 的共享 Redis 上。

3. 理由要订正：v0.3 §5.4 写「失租的写已经在 MySQL 里」，G1 治标落地后不再成立。ordering_lease.go 文件头和 store_failure.go:29-31（HEAD，已随 de4960bee 入库）都写明：失租也可能发生在 SQL 之前，runDBOp 里 lease.beginSQL 失败时一条 SQL 都没发（key_ordered_consumer.go 约 :2404-2409）。所以因失租进死信的条目不一定已经落库，不能当作无害。这让第 4 项的死信工具成为「不豁免」的必要配套。

4. 续租现在有一轮最多 10 次的有界退避（ordering_lease.go 的常量段），能连续失租 3 次的门槛已经很高。
    **blocking**: §5.4 的写法，以及 §11 第 2 步缓存脚本用例的期望值（耗不耗预算）。选豁免的话，要改 placement_route.go 的 retryBudgetExempt，而 go/db 会话的 R2 正在改同一片代码，需要排期协调。
  - [3]
    **no**: 4
    **title**: Stage 1b 依赖 L-04 第 2 批死信工具（§14 第 9 条）
    **context**: 围栏真正起作用，要把 zone 切到 dual 模式（Stage 1b）。dual 下，被围栏判为「旧主写」「计数器倒退」「无版本写」的存盘会进死信。死信现在没有任何查看和重放工具，L-04 又明令不许用 redis-cli 把条目搬回去。所以设计把「L-04 第 2 批死信工具落地并演练」定为切 dual 的前置，而第 2 批目前还没授权。
    ## options
      - [0]
        授权 L-04 第 2 批死信工具：能列出、查看、重新入队（重试次数清零），默认只演练；新进死信的条目带原因信封。落地并演练后再切 dual
      - [1]
        不授权：dual 一直不切，围栏列只写不判，等于没上线
      - [2]
        先切 dual，死信靠人工 redis-cli 处理（L-04 §3.8 与告警描述都已禁止，不建议）
    **recommendation**: 授权第 2 批死信工具，由 go/db 存盘管线线来做，排在它的 R2（④）、R3 之后，本线 Stage 1b 之前。
    **rationale**: 1. 有死信队列就要有查看和重放工具，这是业界标准做法。L-04 自己的 §11 第 4 条也建议死信工具立项；PROGRESS 2026-10-14 条目写着「L-04 第 2 批未做，L-04 仍未关闭」。

2. 第 3 项选了不豁免之后，因失租耗尽预算进死信的条目可能根本没落库，必须能重放。

3. 工具要和 ④ 对齐：
- ④ 把 ready 拆成共享收件箱加 p{N}（S1 规格），重新入队应写进共享收件箱，并走 WAITAOF 入队；
- go/db 会话在 R2 里新增的 cmd/retry_requeue 只负责把 p* 搬回共享列表，不是死信工具，替代不了。

4. 与在途实现冲突：在途 go/db 把被拒的写一律 ack 丢弃（key_ordered_consumer.go:2431-2434，未提交），根本不进死信，看起来也就不需要工具。但代价是：§9.1 场景下计数器倒退时会静默丢盘，这正是 B1 要修的问题。
    **blocking**: Stage 1b（逐 zone 切 dual），也就是围栏判定在生产环境真正生效的那一步。
  - [4]
    **no**: 5
    **title**: 冷登录与在途写的竞态（§14 第 15 条）
    **context**: 玩家登录时，如果 Redis 里的玩家整包（父键）不在了，login 会用子表缓存或 MySQL 重新拼一份。如果这时该玩家还有存盘没落库（比如库不可用期间被延后在重试队列里），拼出来的就是旧数据；C++ 加载后再存盘，会把在途的新数据盖掉。这个问题今天就存在，NX 和条件写都挡不住。
    ## options
      - [0]
        本批不做，只登记（v0.3 现状）
      - [1]
        go/db 按 key 维护「有待落写」标记：写进 ready 时置上，ack 时清除；读任务看到标记就快速失败，让 login 重试
      - [2]
        login 冷组装前先查同一个标记
    **recommendation**: 单独立项，采用「go/db 维护待落写标记 + 读任务快速失败」，归 go/db 存盘管线线，排在 ④ 之后。进了死信的写不清标记（fail-closed），由死信工具处置完再清。过渡期在 runbook 里写明：禁止手工删 PlayerAllData 父键。
    **rationale**: 1. 入口已经变窄：
- ① 读写分区对齐已经落码（A1 / C1；go/login/internal/kafka/key_ordered_producer.go 改用契约分区器；已入库，未验证）。在 Kafka 里排队的写会先于同 key 的读被处理；
- 剩下的窗口只有两种：写已被延后进了 ready（L-04 存储不可用），或者已经进了死信。现实的触发口是手工删父键（§6.2 残余窗口 3）。

2. 为什么排在 ④ 之后：标记必须和 ready 的入队、出队在同一段 Lua 里原子维护，而 ④ 正在重写 ready 的结构和认领脚本（S1 规格），提前做会被推翻。

3. 为什么放在 go/db 读任务侧，不放 login：判定和数据在同一个服务、同一个 Redis 上，login 不需要知道 go/db 的内部键（信息隐藏）。读失败时 login 本来就有重试路径（沿用 L-04「读任务快速失败」的口径）。

4. 为什么进死信不清标记：死信里的写同样没落库，清掉标记就等于放行回档。代价是这名玩家的登录会一直被卡到死信处置完，需要第 4 项的工具配合清标记。这是按 AGENTS §11.3「玩家资产与数据完整性默认 fail-closed」做的取舍。
    **blocking**: 不阻塞围栏本批。阻塞的是「父键丢失后冷登录不回档」这条保证，以及 §9.1 / §9.2 的运维流程能不能承诺「恢复后可以直接放玩家登录」。
  - [5]
    **no**: 6
    **title**: epoch-raise 入口放在哪（§14 第 5 条；并入第 20 条余量 K 与收件箱 C4）
    **context**: Redis 里玩家的 owner_epoch 计数器一旦倒退（从备份恢复、丢了 AOF 尾巴），之后的存盘会被当成旧主写而拒绝。处置办法是人工把计数器抬到所有地方见过的最大 epoch 之上，也就是 epoch-raise。要一起定的有：工具放在哪；从哪些地方取最大值；收件箱的高水位怎么取（收件箱的 C4）；抬升时加余量 K，还是先排空积压。
    ## options
      - [0]
        位置 A：给 go/db/cmd/migrate 加子命令
      - [1]
        位置 B：独立的 go/db/cmd/epoch_raise
      - [2]
        位置 C：做成 scene_manager 或 data_service 的管理 RPC
      - [3]
        收件箱 a：工具直接连收件箱的库
      - [4]
        收件箱 b：工具加 -floor 参数，runbook 先调 GetSummary 再把值传进来
      - [5]
        收件箱 c：工具自己调 PlayerInboxAdmin.GetSummary 取 max_fence_epoch
      - [6]
        余量：K 取一个常数，或者先排空该 zone 的积压与重试队列
    **recommendation**: 位置 B（独立 cmd），收件箱 c，余量用常数 K（建议 10^6 量级）。具体做法：
- 工具自己收齐全部来源，任一来源读不到默认拒绝执行：各落点库两张玩家表的 fence_owner_epoch、各 topic 世代的 consumer:applied_epoch、收件箱的 max_fence_epoch（经 GetSummary）、在途资产 SQL 提交证据 player:{id}:asset_sql_commit 里的 epoch；
- -floor 只作为未知来源的补充入口保留；
- 不要求先排空；
- 默认只演练，带 -confirm 才写；用只升不降的 Lua，输出抬升前后的值和每个来源的取值。
    **rationale**: 1. 为什么不进 migrate：migrate 管表结构，epoch-raise 是数据修复，职责不同（SRP）；合在一起还会让 migrate 的权限和配置变复杂。

2. 为什么放 go/db 而不是 data_service 或 scene_manager：applied_epoch 的键格式、topic 世代、落点库注册表都是 go/db 的内部知识，放到别处就得把这些细节泄漏出去。

3. 为什么收件箱走管理 RPC，不直连库：跨服务只经对方公开的接口读数据；GetSummary 本来就是为此加的（proto/player_inbox/player_inbox_admin.proto:24-25、:80，已入库）。-floor 要人工抄值，漏一步就会在收件箱侧再触发 fence_regressed，是可以避免的人工步骤。

4. 资产证据是在途实现新增的第四个来源：go/shared/assetop/sql_commit.go:13-15（未跟踪）把 (epoch, seq) 存在 Redis。计数器倒退后新铸的 epoch 会低于它，证据发布脚本（:18-26）返回 0，资产终结就会卡住。在途资产通道如果保留，必须把它纳入。

5. 为什么余量用大常数而不是先排空：
- epoch 是 uint64，全仓对 epoch 只做大小比较（go/db 守卫；收件箱 DecideFence，go/player_inbox/internal/inbox/fence.go:35-50；C++ 的 CAS 是等值比较），跳号无害；
- Lua 用 double 比较，2^53 以内精确，加 10^6 没有影响；收件箱的上界 R 就是计数器本身，也不会误判 EPOCH_AHEAD；
- 先排空要停整个 zone 的存盘管线，代价远大于收益；
- 第 11 项如果采纳「铸造带墙钟下限」，K 可以直接取当前毫秒，这个二选一就不存在了。

6. 需要同步改的契约：go/shared/ownerepoch/ownerepoch.go 包注释里「只经 scene_manager 的 INCR 前进」（HEAD，约 :17-22）要补上运维写入方；scene-owner-reentry-barrier.md 里「唯一写入方」的表述同改。
    **blocking**: §9.1 runbook；AGENTS.md §6.2 的修改（负责人已定落码时加）；收件箱遗留项 C4（player-inbox-handoff-20261015.md §3 第 3 步）；合服 runbook §9.2 与回档文档里「抬计数器」那一步。
  - [6]
    **no**: 7
    **title**: proto 生成怎么跑才不写客户端，陈旧 sql 是否刷新（§14 第 6 条）
    **context**: 三个 proto 改完后要重新生成代码。默认生成配置会顺手写同级的客户端仓库，而本批不是客户端任务。另外，生成器路径上还有三份 mysql_database_table.sql，停在 9 月 9 日，缺新加的列，要定的是要不要随本批刷新。
    ## options
      - [0]
        生成：用默认配置
      - [1]
        生成：带 -ConfigPath proto_gen.no-unity.yaml
      - [2]
        sql：随本批刷新并提交
      - [3]
        sql：本批不碰，生成时若被改动就还原
      - [4]
        sql：另开任务决定去留（生成器还会产出就同步提交；已不再产出就删掉文件，并改掉引用它们的注释）
    **recommendation**: 生成用 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`，需要时先跑 proto-gen-build。AGENTS.md §4.1 已经订正过，只需把设计 §11 第 1 步的过时说法改掉。陈旧 sql 本批不碰，另开小任务决定去留。
    **rationale**: 1. 已核实的现状：
- AGENTS.md §4.1 已写明没有客户端授权时用 no-unity 配置（ab0f27907，2026-10-15）；
- 该配置 enable_unity_client: false（proto_gen.no-unity.yaml:233），其余内容与默认配置一致（两份文件的在途改动都加了 mail / rank 的 domain_meta，只有注释不同）；
- dev_tools.ps1 支持 -ConfigPath（:6、:419-437）；
- 本批三个 proto 都不是客户端可见契约（设计 §13）。

2. 三份 sql 的现状：
- 生成器的 GenerateDBResource 会写 go/<域>/model/mysql_database_table.sql（tools/proto_generator/protogen/internal/generator/go/db_model.go:173-200），但这三份从 ffa432946（09-09）起就没变过；
- 工作区里已经有人重新生成过一次（go/proto/db/db_task.pb.go 已含 SaveSeq，未提交），三份 sql 仍然没变，推断生成器实际已不再刷新它们（原因未核实）；
- 部署和脚本都不读这三份文件，只有注释引用（如 go/db/internal/migrate/plan.go:244）；表结构的唯一权威是 cmd/migrate 的 Drift。

3. 为什么不随本批刷新：刷新会把 settlement_ledger、inbox_ledger 等别人的列一起带进来，违反「不夹带任务外改动」。生成物要么与源同步、要么不存在，所以另开任务二选一。

4. 在途那次生成不完整：robot/vendor/proto/db/db_task.pb.go 和 generated/robot/proto/db/db_task.proto 里还没有 save_seq，而它们在设计 §11 第 1 步的 22 个生成物清单里。本批生成后要按清单逐个核对。
    **blocking**: A 批（proto 与生成物）的执行，以及 Codex 的通过标准（§11 第 1 步要求 git diff 只出现那 22 个生成物）。
  - [7]
    **no**: 8
    **title**: 新旧 critical 告警重复（§14 第 16 条）
    **context**: 条件写进死信的三类条目（旧主写、计数器倒退、无版本写）各有一条专用的 critical 告警；同时它们也会触发 L-04 的通用告警 DbTaskDeadLetterAdded（warning）。同一件事会响两次，要定的是要不要去重、怎么去重。
    ## options
      - [0]
        接受重复
      - [1]
        在 L-04 的通用规则里排除这三个 reason
      - [2]
        用 Alertmanager 的 inhibit 规则让 critical 压住 warning
    **recommendation**: 在 DbTaskDeadLetterAdded 的表达式里加 `reason!~"fence_stale_owner|fence_legacy_after_seq|owner_epoch_regressed"`，先与 L-04 的维护者（go/db 存盘管线线，R5 正在改同一个文件）对齐，再同批修改。
    **rationale**: 1. 改动小、语义清楚：现有规则按 (namespace, reason) 聚合（deploy/k8s/db-task-retry-alerts.yaml:73-75，HEAD），加一个 reason 过滤就是一行改动，每类死信只归一条告警。

2. inhibit 方案没有落点：仓库里没有 Alertmanager 配置，deploy/ 与 tools/scripts 下都 grep 不到 inhibit_rules。

3. 接受重复会让值班先看到 warning，低估严重度。DbStaleOwnerWriteRejected 已经有过被当成良性告警的先例（§5.1）。

4. 第 1 项若采纳方案 3，新增的「同版本冲突」reason 要一并排除，并单独配一条 critical。

5. 注意在途实现：它不把被拒的写送进死信（见第 4 项），按在途的形状，这几条告警永远不会响。
    **blocking**: D 批的告警文件（owner-epoch-alerts.yaml、db-task-retry-alerts.yaml），以及 §8.3 第 4 条告警演练的通过标准。
  - [8]
    **no**: 9
    **title**: 进程换代是否必铸 epoch（§14 第 8 条）
    **context**: scene 进程崩溃后，它的节点编号可能被新进程复用。scene_manager 只按节点编号判断是不是同一节点，同一节点就不铸新 epoch。于是新旧两代进程可能共用同一个 epoch，序号先后只能靠墙钟，最坏情况下新进程的存盘会被判成旧写而丢掉。要定的是：要不要让 scene_manager 在进程换代时必定铸造新 epoch。
    ## options
      - [0]
        A：先核实这条时序是否可达，可达再做（v0.3 的写法）
      - [1]
        B：直接做。PlayerLocation 记录节点实例身份（进程 UUID），实例变了就按「持有者已换」铸造
      - [2]
        C：不做，靠墙钟下限加主机时钟偏差监控
    **recommendation**: B，归跨 zone 归属 / scene_manager 那条线。作为多主机环境切 dual 的前置；单机 dev 不要求。
    **rationale**: 1. 这是围栏令牌的基本规则：持有者每换一次（进程重启也算），令牌必须严格变大。只按节点编号判同一持有者，本身就违反这条。

2. 按阅读推断，这条时序可达（未实测）：
- samePhysicalNode 只比较 zone 与 node_id（go/scene_manager/internal/logic/enterscenelogic.go:523-524，HEAD）；
- PlayerLocation 没有实例字段（proto/scene_manager/storage.proto:17-26）；
- 死节点接管要求该节点已从 etcd 注册表消失（playerLocationOwnerDead，约 :869-922）。新进程一旦用同一个 node_id 重新注册，接管路径不成立，samePhysicalNode 反而成立，于是走不铸造分支；
- K8s 的 CrashLoopBackOff 可能把重启拖过 180 秒租约，重启后拿到同一编号是现实时序。

3. 在途实现让这件事更紧迫：
- 在途 C++ 是每玩家计数，没有墙钟下限（见第 2 项），新进程从父键播种；
- 父键落后于已发出的 DBTask 时，新进程发出的序号会与旧进程撞号（在途 go/db 判为同版本冲突并报错），或者更小（在途 go/db 判 errPlayerRowStale，直接 ack 丢弃，key_ordered_consumer.go:2431-2434，未提交）；
- v0.3 的进程级墙钟下限能把这个风险压到「分钟级时钟回拨」才会触发，在途形状没有这层保护。

4. 实现要点：判断「实例变了」要有正面证据（注册表里该 node_id 的实例 UUID 与 location 里记的不同），并继续走再入屏障，不能把「暂时看不到」当成换代。
    **blocking**: 多主机环境的 Stage 1b（§8.3 第 7 条）。如果选在途的每玩家计数形状，它会直接成为 Stage 1c（上新版 C++）的前置。
  - [9]
    **no**: 10
    **title**: G4a 拆分后 go/db 的 Redis 客户端（§14 第 19 条）
    **context**: 共享 Redis 计划按用途拆成几个实例（G4a）。拆分第 2 批会把 go/db 的游标、已落库 epoch、排序锁搬到 redis-mapping，玩家的 owner_epoch 计数器则留在核心实例。围栏的计数器倒退探测要同时读这两边，所以 go/db 需要再加一个指向核心实例的客户端。
    ## options
      - [0]
        go/db 新增一个只读客户端 OwnerEpochRedis，指向核心实例
      - [1]
        applied_epoch 留在核心实例，不搬
      - [2]
        拆分后放弃探测
    **recommendation**: 新增这个客户端，只做 GET owner_epoch，epoch-raise 也共用它。同时在 G4a 设计稿里登记两条依赖：一是探测客户端；二是 §6.2 缓存令牌脚本会被拆到两个实例（详见理由）。G4a 第 2 批要等第二条解决后才能切。
    **rationale**: 1. 为什么选新增客户端：
- 探测只在判 stale 时发生，很少；多一个连到核心的只读客户端成本很低；
- applied_epoch 是每个写任务都要读的高频键，留在核心违背 G4a 给核心减负的目的；
- 放弃探测会让计数器倒退重新变成静默丢盘。

2. 新发现的冲突（设计 §14 第 19 条没写到）：
- G4a §3.2.3（docs/design/shared-redis-split-by-purpose.md:285-299）的第 2 批，让 go/db 的 RedisClient（排序锁在这里）指向 redis-mapping，新句柄 CacheRedis（子表缓存在这里）留在核心；
- 围栏 §6.2 的写路径令牌脚本要在同一段 Lua 里读排序锁、写子表缓存，两把键分到两个实例后就没法原子执行；
- 解法二选一：第 2 批之前把缓存栅栏换成 §6.4 的值封装（单键，不依赖锁），或者让排序锁和子表缓存留在同一实例；
- §5.5 的推进脚本（锁、游标、epoch 三把键）都去 redis-mapping，不受影响。

3. 已核实：G4a 稿里没有任何对围栏设计的引用（grep db-write-fencing、计数器探测，都没有结果）。

4. 在途资产 SQL 提交证据 player:{id}:asset_sql_commit 也要登记：go 侧经 go/db 的 redisClient 发布，data_service 经 Router.ClientForPlayer 读取，拆分后两边是否还在同一实例未核实。
    **blocking**: G4a 第 2 批（go/db 控制面迁到 redis-mapping）的上线。不阻塞围栏本批：今天这三族键都在同一个实例上（§5.1，[C]）。
  - [10]
    **no**: 11
    **title**: 铸造的持久下限（§14 第 7 条）
    **context**: owner_epoch 只存在 Redis 里，Redis 一回退，计数器就回到旧值。设计里的探测只能发现问题，epoch-raise 只能人工修。根治要让「铸造新 epoch」这一步本身就不会低于历史值，也就是给铸造加一个持久的下限。交接里原说并入 D1（映射落库），但 D1 提案里没有这一项。
    ## options
      - [0]
        A：铸造带墙钟下限，新 epoch = max(当前 + 1, 当前毫秒)。Redis 回退后自动越过所有历史值，不需要新存储
      - [1]
        B：把计数器持久化到 MySQL / TiDB（并入 D1，或单独一张表）
      - [2]
        C：铸造时读 MySQL 行内围栏取 max
      - [3]
        D：不做，靠 §5.1 的探测加 epoch-raise 人工处置
    **recommendation**: A，归 scene_manager / 跨 zone 归属线单独立项。不阻塞 Stage 1b，做完之前按 D 运行。
    **rationale**: 1. A 为什么成立：毫秒数在 Redis 回退前后都只会往前走。只要 scene_manager 主机的时钟回拨不超过回退的跨度，回退后第一次铸造就会超过所有用过的 epoch，探测和 epoch-raise 退化为兜底。

2. 仓库里有同类先例：帮会资产流的纪元就是「建行时写入当前毫秒」（docs/design/guild-phase2/04-asset-channel.md:1170、:1219）。

3. 不用改格式：值约 1.8e12，远低于 Lua double 的精确上限 2^53，全仓对 epoch 只做大小比较。

4. 为什么不选 B、C：B 是教科书做法，但要在 EnterScene 热路径上加一次持久化写，或者做复杂的异步落库；C 让 scene_manager 依赖玩家库的落点，耦合重而且在热路径上。两者都不划算。

5. A 的改动点（落码时核对）：
- luaMintEpochAndSetLocation 的重放识别依赖「恰好 +1」（go/scene_manager/internal/logic/owner_epoch.go 约 :58-118，HEAD），时间要由调用方经 ARGV 传入，重放才保持幂等；
- 回滚用的 INCR 不受影响；
- 收件箱的上界 R（fence.go:21-28）仍然成立。

6. 采纳 A 后，第 6 项的余量 K 可以直接用当前毫秒，不用再拍常数。
    **blocking**: 不阻塞本批。阻塞的是「Redis 回退后不需要人工就能恢复」这条长期保证，以及 AGENTS.md §6.2 能不能去掉「只清 Redis 时先抬计数器」这一人工步骤。
  - [11]
    **no**: 12
    **title**: 两个可选项（§14 第 17、18 条）
    **context**: 第 17 条：死信里同一玩家同一张表只保留版本最大的一条，让死信内存按玩家数封顶。第 18 条：旧版 C++ 的写（class-2）落库后，把库里已有的序号回填进缓存，让缓存里的版本更准。两项设计都标为「可选，默认不做」。
    ## options
      - [0]
        第 17 条：做 / 不做
      - [1]
        第 18 条：做 / 不做
    **recommendation**: 两项都不做。
    **rationale**: 1. 第 17 条：
- 死信是证据库，去重会丢掉处置要用的中间载荷，§9.1 的「定真源」要看倒退期间到底写了什么；
- 还要换数据结构，并同步改合服预检 tools/merge_zone/preflight.go 和 db_retry_queue_length{queue="dead"} 的采样，这两处都按单一列表工作；
- 死信增长有 critical 告警和「按小时处置」兜着，等真实数据显示内存成问题再议（YAGNI）。

2. 第 18 条：class-2 只在 C++ 回滚窗口出现；缓存里的围栏本来就定义为下限，回填只让播种下限准一点，收益很小。

3. 与在途实现的冲突，比这两项要不要做严重得多：
- 在途 go/db 没有 class-2 的两步写，而是把 save_seq=0 的写一律当成无版本写（player_row_fence.go:28、:32，未跟踪）；
- 行上已有围栏时判 errPlayerRowStale 并 ack 丢弃；
- 结果是 C++ 一旦回滚到旧版，玩家的所有存盘都会被静默丢弃；
- 如果保留在途实现，必须先补上 §4.3 的 class-2 两步写。
    **blocking**: 没有硬阻塞。定下来后，可以从 §11 第 2 步删掉对应的可选用例，§9.4 也可以定稿。
  - [12]
    **no**: 13
    **title**: 两项另开任务的待核项（§14 第 10、11 条）
    **context**: 第 10 条：Java 版里同一玩家、同一 epoch 重复进场时，会换新实例并沿用旧内存；旧实例在途的存盘可能晚于新实例提交，把库写回旧值。这是 Java 自己的缺口。第 11 条：压测开关 STRESS_TEST_PROBE=1 时，存盘的「无改动跳过」快路径疑似永远不命中，影响压测口径。
    ## options
      - [0]
        各自另开只读核实任务
      - [1]
        并入本批
      - [2]
        不处理
    **recommendation**: 两项都另开任务。第 11 条排在下一轮压测之前；第 10 条交给 Java 线，按 AGENTS §12 登记处理。
    **rationale**: 1. 第 11 条已被在途改动部分改写：
- 在途 C++ 把快路径的比较从 dirty_save::IsEqual 换成了 player_exit::IsPersistedPayloadCurrent（player_lifecycle.cpp:4313，未提交）；
- 后者在两份副本上都清掉 stress_test_probe 和围栏字段（:814-829，未提交）；
- 按阅读推断，这顺带修好了「探针模式下快路径永不命中」，但会让探针模式下的跳过率从约 0 回到正常值，与历史压测基线不可比；
- 所以核实任务要做两件事：确认修复成立；在下一轮压测复盘里写明跳过率口径的变化（AGENTS §6 要求有对比表）。

2. 第 10 条：Java 侧证据在 Java 仓的 SceneWorld.java:1403-1424（设计 §13 引用，未在本仓核实）。它不影响本仓围栏，按 AGENTS §12 在 Java 仓 PARITY.md 登记，由 Java 线决定是否修。
    **blocking**: 第 11 条会让下一轮压测结论不可比（AGENTS §6.2 的 prev-summary 对比）。第 10 条不阻塞本仓任何步骤。
**notes**: 一、先决问题（建议在 13 项之前先拍）：围栏实现以哪一版为准

工作区里有一套在途的围栏实现，来源是「全域到账续批」资产会话（见 docs/design/guild-phase2/04-asset-channel.md 文末 2026-10-15 一段，未提交）。涉及的文件：
- go/db/internal/kafka/player_row_fence.go（未跟踪）；
- go/db/internal/kafka/key_ordered_consumer.go:952-954、:961-970、:2431-2434（未提交）；
- cpp/libs/services/scene/player/system/player_lifecycle.cpp 的每玩家 save_seq 发号（:4132、:4371-4388，未提交）；
- proto 已加 DBTask.save_seq=9、PlayerAllData.last_save_seq=7，并已部分重新生成（未提交）。

它与 v0.3 有六处实质分歧：
1. 没有 legacy / dual 模式开关，与 §14 第 4 条「缺省 legacy、按 1a→1c→1b 逐 zone 切」直接冲突。第 4 条不在 13 项里，但在途实现绕过了它。
2. 用 FOR UPDATE 事务加 Save，不是 SaveConditional。每笔玩家存盘多一次 SELECT … FOR UPDATE 和事务往返，热路径代价没有压测数据（AGENTS §6）。
3. 被拒的写一律 ack 丢弃：不复核落点，不进死信，还照样推进游标和已落库 epoch（runDBOp 返回 nil 后，handleTask 在 :1920-1924 推进），违反 F-2 / F-3。计数器倒退（B1）时会静默丢盘。
4. class-2 / class-3 在已有围栏的行上被静默丢弃。C++ 一旦回滚到旧版，所有存盘都会丢。
5. C++ 用每玩家计数，没有墙钟下限、没有上界，饱和后该玩家永远存不了盘。
6. 违反交接 §3.4 的约定「go/db 会话发文件清单之前不碰 go/db/internal/kafka」，也跳过了「SaveConditional 先经 Codex 验证」这一前置。

第 1、2、4、8、9、12 项怎么落，都取决于这个先决选择。建议以 v0.3 为准，把在途实现收敛过去，同时吸收它的两点正确要求：「同版本不同载荷必须失败」（即第 1 项方案 3），以及资产 SQL 提交证据对 (epoch, seq) 的依赖（第 6 项来源、第 10 项落点）。

二、编号对照

本备忘按交接 §3.4 的 13 项编号；括号里是设计 §14 的原编号，以原文为准：
- 1 → 12；2 → 13；3 → 14；4 → 9；5 → 15；
- 6 → 5（并入第 20 条「余量 K 还是先排空」，以及收件箱的 C4）；
- 7 → 6；8 → 16；9 → 8；10 → 19；11 → 7；
- 12 → 17、18；13 → 10、11。

§14 第 1–3 条已定，第 4 条见上文第一部分。

三、仍未核实

- proto2mysql 的 SaveConditional 仍未经 Codex 验证；
- 在途实现 proto.Equal 比对时，各列类型往返是否丢精度；
- 生成器为什么不再刷新三份 mysql_database_table.sql；
- 第 9 项的可达性只是按阅读推断；
- 资产证据键在 G4a 拆分后落在哪个实例；
- Java 侧的 SceneWorld 行号。

四、本次工作边界

本备忘严格只读：没有修改或新建任何文件，没有编译或运行测试。引用的行号以 2026-10-15 的工作区为准，HEAD 已提交与未提交在途改动在各项里分别注明。
