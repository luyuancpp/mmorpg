# 事故报告:friend 锁定读被规划成索引全扫描,锁集越出守卫,真死锁 1213

- **日期**:2026-09-21(机器 B,`D:\luyuan\wuxingqitan\mmorpg`,时间为本地 -04:00)
- **级别**:P1(产品缺陷,可稳定复现的死锁)。**线上影响:无** —— friend 服务从未部署,本次是它第一次在真 MySQL 上跑并发回归。
- **发现方式**:交接文档 `docs/handoff/friend-handoff-20260920.md` §2 第 6 步"真 MySQL 并发回归"首跑。
- **状态**:已修复,真库验证通过;**截至本文写入时修复代码尚未提交**(以 `git log -- go/friend/internal/data/friend_repo.go` 为准)。〔2026-09-28 更正:✅ **已提交** —— §5.2 的 6 个代码文件与本报告随 2026-09-21 08:52 的自动保存提交 `9cef7b2ec` 进库(`git show 9cef7b2ec` 可见 `lockCapacityRowSQL` / `lockBlockRowSQL` / `lockFriendEdgeRowSQL` / `cancelPendingRequestSQL` / `deleteTerminalRequestSQL` 与 `TestLockingStatementsArePrimaryKeyPointLookups`);同日 22:49 的全仓数据层死锁审计 `ff39a13f1` 在其上补了容量行 / 拉黑写入 ODKU 与 DSN 层 READ COMMITTED〔2026-09-29 更正:归属写错了。容量行补行的 ODKU(`ensureCapacityRowSQL … ON DUPLICATE KEY UPDATE player_id = player_id`)已经在 `9cef7b2ec` 里;`ff39a13f1` 对 `friend_repo.go` 只改了注释,它给 friend 新增的是拉黑写入 ODKU(`block_repo.go` 的 `insertBlockRowSQL`)与 DSN 层 READ COMMITTED(`svc/servicecontext.go` 的 `BuildDSN`)。与 `friend-handoff-20260920.md` §5.6 (b)、§9.5 第 5 条一致〕。09-25 第 7–9 步已跑通,09-28 已在真实数据量上复核 `EXPLAIN`(见 §7.3 / §7.5)。〕
- **一句话**:好友服务的几条 `SELECT ... FOR UPDATE` 写成了 `OR` / `IN`,优化器把它们规划成索引全扫描,锁到了**别的玩家对**的行上,与那一对玩家"先主键后二级索引"的 `DELETE` 取锁顺序相反,成环。守卫只串行化"这一对玩家",所以锁集一旦越界,守卫就失去了意义。

---

## 1. 现象

`go/friend/internal/data` 在真 MySQL 上首跑(`FRIEND_TEST_MYSQL_DSN` + `FRIEND_REQUIRE_MYSQL_TESTS=1`):**71 PASS / 2 FAIL / 0 SKIP**。

```
friend_guard_lock_order_mysql_test.go:678: 容量行回收与写路径并发 触发 InnoDB 死锁(1213):… 严格组 writer 0 第 2 轮 Unblock:
  unblock 87000001->87000002: Error 1213 (40001): Deadlock found when trying to get lock; try restarting transaction
--- FAIL: TestCapacityRowReclaimRacesWithGuardedWrites (13.09s)
```

单独重跑这一个用例,**第一次就复现**,这次撞在另一条路径上:

```
严格组 writer 0 第 20 轮 RemoveFriend: delete friend edge 87000001->87000002: Error 1213 (40001): Deadlock found …
```

另一个失败 `TestAcceptFriend_RejectsBlockedPair` 与死锁无关,是测试夹具自相矛盾(见 §7.4)。

**环境**:`mysql:latest` 实为 **MySQL Community Server 26.7.0**(不是 TiDB);全局 `REPEATABLE-READ`,friend 写事务显式 `READ COMMITTED`;`binlog_format=ROW`;`innodb_deadlock_detect=1`。测试库 `mmorpg_friend_test`,表由测试夹具建,数据量极小(每个用例几十行)。

## 2. 时间线

| 时刻 | 事件 |
|---|---|
| 09-19 | friend 移植 F2 批(`f06090b19`)写下 `blockedEitherWay` / `friendEdgeExistsForUpdate` 的 `OR ... FOR UPDATE`、`lockCapacityRows` 的 `IN (...) FOR UPDATE`、`Block` 取消 pending 的 `OR` 形 `UPDATE`、sweep 的批量 `DELETE ... LIMIT`。整批从未编译、从未运行 |
| 09-20 | 交接文档 §3 第 2 条登记:"三条锁序假设从未在真 MySQL 上 `EXPLAIN` 核对 —— `OR` 形式必须走 index_merge / range 且 key 含 PRIMARY,退化成全索引扫时 `FOR UPDATE` 的锁集当场越出守卫域,ABBA 立刻可能成环"。**风险被准确预言,但无人核对** |
| 09-20 | 收尾批新增并发场景 `TestCapacityRowReclaimRacesWithGuardedWrites`(多个写者各占一对玩家,混跑 AddFriend / AcceptFriend / RemoveFriend / Block / Unblock + 回收) |
| 09-21 约 05:39 | 首次真库回归,该场景 1213 失败(撞在 Unblock) |
| 约 05:42 | 单独重跑第一次即复现(撞在 RemoveFriend 删边);抓取 `SHOW ENGINE INNODB STATUS` 的 `LATEST DETECTED DEADLOCK`(§3.1) |
| 约 05:44 | 对全部锁定语句跑 `EXPLAIN`(§3.2),确认机制,并发现另外三处同类问题 |
| 约 05:45–05:49 | 修复(§5)+ 确定性回归用例 |
| 约 05:50 | `internal/data` 真库重跑 74 PASS / 0 FAIL / 0 SKIP |
| 之后 | 8 个并发锁序场景连跑 5 轮 40/40 PASS;`go/friend` 全部包 314 PASS / 0 FAIL / 0 SKIP |

## 3. 根因(逐环,均有证据)

### 3.1 死锁现场(`LATEST DETECTED DEADLOCK` 原文,去掉十六进制行)

```
*** (1) TRANSACTION:
TRANSACTION 40426, ACTIVE 0 sec updating or deleting
DELETE FROM friend_block WHERE player_id=87000141 AND blocked_player_id=87000142

*** (1) HOLDS THE LOCK(S):
RECORD LOCKS … index PRIMARY of table `mmorpg_friend_test`.`friend_block` … lock_mode X locks rec but not gap
Record lock, heap no 5 …

*** (1) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS … index idx_blocked_player of table `mmorpg_friend_test`.`friend_block` … lock_mode X locks rec but not gap waiting
Record lock, heap no 5 …

*** (2) TRANSACTION:
TRANSACTION 40416, ACTIVE 0 sec fetching rows
SELECT 1 FROM friend_block
     WHERE (player_id=87000101 AND blocked_player_id=87000102) OR (player_id=87000102 AND blocked_player_id=87000101)
     LIMIT 1 FOR UPDATE

*** (2) HOLDS THE LOCK(S):
RECORD LOCKS … index idx_blocked_player of table `mmorpg_friend_test`.`friend_block` … lock_mode X locks rec but not gap
Record lock, heap no 5 …

*** (2) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS … index PRIMARY of table `mmorpg_friend_test`.`friend_block` … lock_mode X locks rec but not gap waiting
Record lock, heap no 5 …

*** WE ROLL BACK TRANSACTION (1)
```

读法:heap no 5 就是 **141/142 那一行**。事务 (2) 是 **101/102** 的 `blockedEitherWay`,它手里拿着 141/142 那一行在二级索引 `idx_blocked_player` 上的锁 —— 一个与它毫无关系的玩家对。事务 (1) 是 141/142 的 `Unblock`。

### 3.2 执行计划(`EXPLAIN`,同一个库)

| 语句 | 计划 | 后果 |
|---|---|---|
| `blockedEitherWay`:`SELECT 1 FROM friend_block WHERE (player_id=? AND blocked_player_id=?) OR (…) LIMIT 1 FOR UPDATE` | **Covering index scan on friend_block using idx_blocked_player** + Filter | 二级索引全扫描,逐条加锁 |
| `friendEdgeExistsForUpdate`:同形,`friend` 表 | **Covering index scan on friend using idx_friend_player** + Filter | 同上 |
| `lockCapacityRows`:`… WHERE player_id IN (?, ?) ORDER BY player_id FOR UPDATE` | **Index scan on friend_capacity using PRIMARY** + Filter | 主键全扫描(不成环,但见 3.4) |
| `Block` ⑥:`UPDATE friend_request … WHERE status=? AND ((…) OR (…))` | `type=range key=idx_status_updated key_len=4` | 扫**全服** pending 行,先二级索引后主键 |
| 主键点查 `SELECT 1 FROM friend_block WHERE player_id=? AND blocked_player_id=? FOR UPDATE` | `const` | 只锁那一行主键 |

两个二级索引恰好都是**覆盖索引**(`idx_blocked_player (blocked_player_id)` 在 InnoDB 里隐含主键列,等价于 `(blocked_player_id, player_id)`,查询只用到这两列),表又很小,优化器判断"扫整个覆盖索引"最便宜。

### 3.3 成环机制

1. `SELECT ... FOR UPDATE` 经二级索引扫描时,InnoDB 对**每条扫到的**二级索引记录加锁,再去锁对应的聚簇(主键)记录,之后才在 MySQL 层判断 WHERE。RC 下不匹配的行会在判断后释放,但**加锁这一步必须先等到** —— 而 `SELECT ... FOR UPDATE` 不走 semi-consistent read(那是 `UPDATE` / `DELETE` 才有的),所以会真的去等。取锁顺序:**二级索引 → 主键**。
2. `Unblock` / `deleteFriendEdges` 是按主键的 `DELETE`:先锁聚簇记录,再锁该行的二级索引项。取锁顺序:**主键 → 二级索引**。
3. 两者撞在同一行上,顺序相反 → 环。
4. 容量守卫为什么没挡住:守卫只让"涉及同一个玩家"的事务串行。101/102 的事务和 141/142 的事务**不共享任何玩家**,守卫本来就不管它们 —— 它们之所以相遇,完全是因为锁集越过了自己那一对。代码注释里"这两行都在'本对玩家'范围内……所以锁集不会跨对交叉"的论证,只在执行计划是主键点查或 index_merge 时成立。

### 3.4 同类但未直接成环的两处

- `lockCapacityRows` 的主键全扫描按主键序取锁,所有守卫的获取都是升序,因此不成环;但每个事务都要**依次等到所有更小 `player_id` 上正被别人持有的守卫行**,等于按玩家号把全部好友写路径串行化 —— 吞吐问题,且对延迟不可预测。
- `Block` ⑥ 的 `UPDATE` 和 sweep 的批量 `DELETE`:先二级索引后主键,与 `AddFriendRequest` 的 upsert(先主键,`④` 的 `FOR UPDATE`)在同一行上反序。前者锁全服 pending 行;后者只在"玩家重新申请一个保留期外的旧终态行、恰逢 sweep 正在删它"时撞上,而且 sweep 一次删上千行、undo 大,InnoDB 更可能牺牲**玩家那一侧**。

## 4. 这段代码为什么会存在

1. **为省一次往返写成了 `OR`**。原注释:"单条 OR 查询而不是查两次……一次往返即可;RC 下未命中不加任何锁,命中则锁住那一行"。这个论证默认了执行计划是两次主键点查,而 SQL 文本本身并不保证这一点。
2. **风险被预言了,但核对依赖运行环境**。交接文档 §3 第 2 条准确写出了"退化成全索引扫就会越出守卫域",但它是"运行期核对"项;整批代码此前从未编译、从未连过真库(CI 不设 MySQL DSN,真库用例"SKIP 在报告里等于绿")。
3. **原有的 5 个锁序场景抓不到它**。它们要么共享同一个目标玩家(守卫本来就会串行),要么用互不相干的目标但不混入 `Unblock` / 删边;要让锁集越界的扫描与另一对玩家的主键 `DELETE` 撞在同一行上,需要"多对玩家并发 + 删除操作"。收尾批新增的回收并发场景恰好满足这两点。

## 5. 修复

### 5.1 规则(写进 `go/friend/internal/data/friend_repo.go` 顶部锁序说明 (6))

**守卫之后的锁定读 / 锁定写,一律写成"完整主键的等值点查 / 点更新",不许用 OR / IN / 前缀范围。** 完整主键等值的执行计划是 `const`(`UPDATE` 为 `key=PRIMARY`、用满全部主键列),与统计信息无关;RC 下未命中不加锁,命中只锁那一行的聚簇记录、**不碰任何二级索引项** —— 于是与"先主键后二级索引"的写不可能反序。

⚠ **前提:WHERE 不能同时把另一个唯一索引的全部列也钉死。** 若表上还有唯一二级索引、且它的列也全部出现在等值条件里,优化器在两个 `const` 路径之间任选其一,可能走唯一二级索引(先锁二级索引项、再锁主键),规则就失效。friend 的四张表按 D-14 **零 UNIQUE KEY**,所以本修复成立;而且 §6.2 的回归断言的是 `key = PRIMARY`,真被规划到别的唯一索引上也会变红。这一条是帮会会话核对 `guild_member`(主键 `(guild_id, player_id)` + 唯一键 `uk(player_id)`)时指出的(2026-09-21),推广到别的服务时必须一起检查。

### 5.2 改动清单(6 个文件,均在 `go/friend/internal/`)

| 位置 | 改动 |
|---|---|
| `data/friend_repo.go` `blockedEitherWay` | `OR` → 两次主键点查(常量 `lockBlockRowSQL`),任一命中即返回 |
| `data/friend_repo.go` `friendEdgeExistsForUpdate` | 同上(`lockFriendEdgeRowSQL`) |
| `data/friend_repo.go` `lockCapacityRows` | `IN (...) FOR UPDATE` → 按升序逐个主键点查(`lockCapacityRowSQL`);点查顺序即取锁顺序 |
| `data/block_repo.go` `Block` ③ | 名额 `COUNT(*) ... FOR UPDATE` → 守卫内**普通读**:能让它变大的只有 `Block` 自己,而它必须先持有同一把守卫;RC 每条语句新快照。与 `AddFriendRequest` ⑤⑥ 同一论证。`COUNT(*)` 天然偏好小覆盖索引,留着 `FOR UPDATE` 迟早会在统计信息变化时重蹈覆辙 |
| `data/block_repo.go` `Block` ⑥ | `OR` 形 `UPDATE` → 两条按主键的点更新(`cancelPendingRequestSQL`) |
| `data/sweep_repo.go` `deleteTerminalRequestsBefore` | 批量 `DELETE ... LIMIT` → 候选普通读 + 逐行按主键删(`deleteTerminalRequestSQL`,WHERE 重复终态与截止点作提交点复核),与容量行回收同一写法;出错时带回已删行数 |
| `logic/sweep.go` | 终态清理失败日志带上"看到 / 已删"行数 |
| 测试:`data/friend_guard_lock_order_mysql_test.go` | 新增确定性回归 `TestLockingStatementsArePrimaryKeyPointLookups`(§6.2) |
| 测试:`data/friend_repo_mysql_test.go` | 修 `TestAcceptFriend_RejectsBlockedPair` 的夹具矛盾(§7.4) |

保留不改的:`friendEdgeExists`(`RemoveFriend` 的事务外前置判定)仍是 `OR`,但它是**普通读、不加锁**,不在本规则范围内。

### 5.3 代价

一对玩家的"任一方向"判定从一次往返变成两次;容量守卫从一次往返变成两次;sweep 终态清理一轮至多 `BatchLimit` 次往返(默认 1000,5 分钟一轮)。均可接受。

## 6. 验证

### 6.1 真库回归(MySQL 26.7.0)

| 轮次 | 结果 |
|---|---|
| 修复前,`internal/data` 首跑 | 71 PASS / **2 FAIL** / 0 SKIP |
| 修复前,单独重跑回收并发场景 | 第 1 次即 FAIL(1213) |
| 修复后,`internal/data` | **74 PASS / 0 FAIL / 0 SKIP** |
| 修复后,8 个并发锁序场景 `-count=5` | **40/40 PASS**,场景 (g) 以外零 1213 |
| 修复后,`go/friend` 全部包(真库) | **314 PASS(含子用例)/ 0 FAIL / 0 SKIP**;`gofmt` / `go build` / `go vet` 零输出 |

日志存档(仓外):`D:\luyuan\wuxingqitan\friend-lockorder.log`(修复前首跑,后被修复后的全量结果覆盖)、`friend-lockorder-x5.log`、`friend-alltests.log`、`friend-deadlock.txt`(`SHOW ENGINE INNODB STATUS` 原文)。

### 6.2 确定性回归:`TestLockingStatementsArePrimaryKeyPointLookups`

对**生产代码里同一个 SQL 常量**(不在测试里另抄一份)做 `EXPLAIN FORMAT=TRADITIONAL`,断言 `key = PRIMARY`、`key_len` 用满全部主键列、SELECT 的 access type 为 `const`。覆盖 `lockCapacityRowSQL`、`lockBlockRowSQL`、`lockFriendEdgeRowSQL`、`cancelPendingRequestSQL`、`deleteTerminalRequestSQL`。**旧写法三处都会被它拦下**(§3.2 的计划:`type=index` / `key=idx_blocked_player` / `key=idx_status_updated`)。它不依赖并发时序,每次运行结论相同;并发场景只能按概率撞上这类问题。

### 6.3 附带证实的一条推演

评审时推演、未曾复现的"回收留下 delete-marked 记录后,并发 `INSERT IGNORE` 在 S→X 升级上成环",这次在真库上**复现**了(日志:`[friend] WARN ensure 容量行撞上 1213 … attempt=1/3`),被 `ensureFriendCapacityRows` 的有限重试吸收,场景 (g) PASS。

## 7. 遗留与后续

### 7.1 【待提交】修复代码尚未提交 〔2026-09-28:✅ 已提交〕

6 个文件见 §5.2。交接文档与 `PROGRESS.md` 可能先于代码进库,以 `git log -- go/friend/internal/data/friend_repo.go` 为准。
〔2026-09-28 更正:✅ 已按上面的判据核实 —— `git log -- go/friend/internal/data/friend_repo.go` 里 `9cef7b2ec`(2026-09-21 08:52,自动保存提交「WIP：保存跨服释放、角色外观、公会经济与好友锁序改动」)包含 §5.2 全部 6 个代码文件与本报告;后续 `ff39a13f1`(2026-09-21 22:49,「数据层死锁审计修复(#2–#18)」)再补 ODKU 与 DSN 层 RC。〕
〔2026-09-29 更正:上一条的"再补 ODKU"不准确。容量行补行的 ODKU(`ensureCapacityRowSQL`)已在 `9cef7b2ec` 里,`ff39a13f1` 对 `friend_repo.go` 只改了注释;它给 friend 补的是拉黑写入 ODKU(`insertBlockRowSQL`)与 DSN 层 RC。〕

### 7.2 【待核,不属本服务】其它 Go 服务里同类形状的锁定语句

全仓扫描 `FOR UPDATE`(排除测试与生成物)后,多数已是主键点查;以下几处**形状可疑,需要在真 MySQL 上 `EXPLAIN` 核对**,方法同 §6.2:

| 位置 | 语句形状 | 疑点 |
|---|---|---|
| `go/guild/internal/data/guild_manage_repo.go` `lockMemberPair` | `… WHERE guild_id = ? AND player_id IN (?, ?) ORDER BY player_id FOR UPDATE` | **帮会会话已核实表结构**:`guild_member` 主键 `(guild_id, player_id)`,另有唯一键 `uk(player_id)` —— 除了与 `lockCapacityRows` 同形的全扫描风险,还可能被规划到 `uk_guild_member` 上(二级 → 主键),与按主键删成员行反序 |
| 同文件 `sqlLockMemberRole` | `SELECT role FROM guild_member WHERE guild_id = ? AND player_id = ? FOR UPDATE` | 是完整主键等值,但 `player_id = ?` 同时钉死了唯一键 `uk(player_id)` —— 正是 §5.1 那条前提不成立的情形 |
| 同文件 `sqlLockMyApplications` | `SELECT guild_id FROM guild_application WHERE player_id = ? FOR UPDATE` | **已核实**:`guild_application` 主键 `(guild_id, player_id)`,本句走二级索引 `idx_guild_application_0`,先二级后主键 |
| `go/shared/assetop/seq.go` `AllocateSeq` | `… WHERE player_id = ? AND stream = ? AND stream_epoch = ? AND status = ? ORDER BY seq LIMIT ? FOR UPDATE` | 带过滤的范围锁,理想路径是 `idx_guild_asset_op_2` 的等值前缀,锁集取决于索引选择 |

**处置(2026-09-21)**:帮会 B5b 会话对**本批新增**的锁定 / 写语句照 §6.2 加确定性 `EXPLAIN` 回归;上表四处是既有代码(前三处属帮会 B2,第四处属聚宝斋),已由帮会会话写进它的交接文档**交用户决定**,本批不改。本报告只登记,friend 会话不动 `go/guild` / `go/shared`。

### 7.3 【说明】规模上的结论

修复后的锁定语句都是完整主键等值,执行计划与数据量无关。**非锁定**的候选读(sweep 的两条候选 `SELECT`)在小表上同样被规划成全索引扫描,只影响性能不影响正确性;上线后应在真实数据量下再 `EXPLAIN` 一次,确认走 `(status, updated_ms)` / `(friend_count, created_ms)` 的范围。
〔2026-09-28 更正:✅ 已在真实数据量下完成(未等上线)—— 一次性库 5 万玩家 / 100 万好友边 / 25 万申请 / 5 万拉黑,MySQL 26.7.0:**全部锁定语句都是 PRIMARY 完整主键等值**(SELECT … FOR UPDATE 为 `const`,UPDATE / DELETE 为 `key=PRIMARY` 用满主键列、`rows=1`);两条候选读分别走 `idx_friend_request_1 (status, updated_ms)` 与 `idx_friend_capacity_0 (friend_count, created_ms)` 的覆盖索引 `range`,实际都读 1000 行(`LIMIT`)即止。同一次核对还发现好友推荐的两条查询(不属本事故的锁定语句)扫描量无界,已另行修复 / 修复中。结果表见 `docs/handoff/friend-handoff-20260920.md` §9.4 末段「2026-09-28 执行结果」。〕〔2026-09-29 更正:"修复中"已过期 —— `recommendAnchor` 于 09-28 改为 W=1024 窗口,`RecommendByMutual` 于 09-29 改写(STRAIGHT_JOIN + 五条完整主键 `NOT EXISTS`,随 `ccafe300c` 进库)。两者都未经 Claude 编译,真库用例待 Codex,见 handoff §9.5 第 8 条。数据集那个一次性库是 `friend_explain_scratch`,仍保留在机器 A 上作只读基线。〕

### 7.4 【已修,非产品缺陷】`TestAcceptFriend_RejectsBlockedPair` 夹具自相矛盾

原移植即有。夹具直写 `friend_block`、绕过 `Block()`,故意造出"拉黑与 pending 并存"去测 `AcceptFriend` ② 的守卫内拉黑复核,末尾却调用含"拉黑后不得有 pending"的 `assertFriendInvariants` —— 断言的是夹具自己造的状态。改为逐条调用其余三条不变量,并显式断言"被拒的 `AcceptFriend` 整体回滚、申请行仍为 pending"。

### 7.5 【未做】交接文档 §2 第 7–9 步 〔2026-09-28:✅ 已完成〕

`-migrate` + 常驻启动、缓存手工核对、两区 robot `friend-smoke` 尚未运行。
〔2026-09-28 更正:✅ 已于 2026-09-25 完成(机器 A):第 7a 步空库 `-migrate` 建出 5 张表、零 UNIQUE、二次迁移 0 条语句;第 7b 步常驻启动横幅、`:9180/metrics` 五个指标预建 0 值、etcd NodeInfo 双填、真 Ctrl+C 3.4 s 内优雅退出;第 8 步由单测 `TestVersionedCache_FillsWhenGenerationKeyNeverWritten` 覆盖(09-21 PASS);第 9 步两区 `friend-smoke` 退出 0、`FRIEND_SMOKE_OK … cross_zone=true`,连跑第二轮同样通过。详见 `docs/handoff/friend-handoff-20260920.md` §9.4「进度续(2026-09-25)」。〕

## 8. 教训

1. **锁集由执行计划决定,不由 SQL 文本决定。** 守卫这类"只串行化一小撮行"的设计,必须让锁集与执行计划无关 —— 完整主键等值是唯一不需要信任优化器的写法。"写成 OR 省一次往返"换来的是把正确性押在统计信息上。
2. **小表是最坏情况,不是最好情况。** 小表上优化器最爱全扫描;测试环境与新服刚开时恰恰是小表。
3. **"需要运行期核对"的风险项要有人认领执行时间。** 这条风险在交接文档里被准确预言,却因为"要真库"一直挂着;第一次真库运行就兑现了。
4. **确定性回归优先于概率性回归。** 并发用例能发现问题,但证明"修好了"要靠 `EXPLAIN` 这类每次结论相同的断言。

## 9. 全仓数据层死锁审计与根治(2026-09-21 → 09-29)

本节是 §7.2 那张"待核"表的收口。friend 这次事故之后做了一轮全仓审计(6 个方向并行排查所有加锁 SQL,
每条发现交独立 agent 反驳式复验),确认 18 条;随后分四轮修复,每轮都配独立复审。
**结论里凡是标"实测"的,都来自 2026-09-29 对着本机 Docker 里的真 MySQL 26.7.0(全局 RR、binlog ROW、
innodb_deadlock_detect=ON)跑出来的受控实验;标"推演"的没有真库证据。**

### 9.1 受控实验:三类现场,结论两次推翻静态推演

实验用一个独立小程序(与仓库代码无关)摆出与生产语句同形的交错,每格重复 5~20 次。

**现场一:唯一键上撞"删除标记记录"(名字被释放后立刻被抢)**

| 表结构 | 写法 | 竞争者 id 都小于原主人(生产常态) | 都大于 | 一大一小 |
|---|---|---|---|---|
| `PRIMARY KEY(player_id)` + `UNIQUE(name_norm)` | 普通 INSERT | **5/5 成环** | 2/5 | 5/5 |
| 同上 | ODKU | **5/5 成环** | 0/5 | 2/5 |
| `PRIMARY KEY(name_norm)` + `UNIQUE(player_id)` | 普通 INSERT | 5/5 | 5/5 | 5/5 |
| **同上** | **ODKU** | **0/5** | **0/5** | **0/5** |

→ **推翻了"ODKU 能消掉 S→X 升级环"这个一般性结论**:它只在"新记录的主键排在删除标记项之后"时成立。
唯一 0/15 全清的组合是"被争抢的键当聚簇主键 **且** 用 ODKU",两个条件缺一不可。
`player_name` 正好是最坏情形:`player_id` 由号段分配(小),存量角色是 snowflake(大),新号抢老名字必然走 5/5 那一路。

**现场二:先到者回滚,两个后到者排在它后面**

| 争抢的键 | 普通 INSERT | INSERT IGNORE | ODKU |
|---|---|---|---|
| 聚簇主键 | 10/10 | 10/10 | 10/10 |
| 二级唯一索引(自增主键) | 10/10 | 10/10 | 10/10 |

→ 成环的是**回滚时的锁继承**(等待锁被继承成间隙锁,两个后到者各自申请插入意向锁互相挡住),
与写法、与键的位置都无关。**ODKU 对它无效**。手册那句 "an exclusive lock rather than a shared lock"
管的是现场一,不是这一类。唯一的消法:**别让两个插入者同时排在同一条未提交记录后面** ——
让所有首次插入者先排在一行已提交的守卫记录上(trade 的哨兵行)。

**现场三:改主键会不会引入新环(同一 player_id 并发写两个名字 / 写一个删一个)**:两种表结构各 0/20,未复现。

### 9.2 可 grep 的判据

**当"被争抢的键"是二级唯一索引、且新记录的主键可能小于删除标记那条记录的主键时,ODKU 不够用。**

按这条扫过全仓:`player_name` 中招(已根治);`zone_whitelist` 自增主键、新项必然更大,ODKU 够用(实测绿);
`zone_config` / `trade_favorite` / friend 各表争抢的键就是聚簇主键,安全;
`guild`(主键 `guild_id` + `uk_guild(name_norm)`)形状与 `player_name` 相同,**安全性依赖 `guild_id` 单调递增**
—— 一旦改成号段分配,这个环立刻回来(已知会帮会会话)。
〔2026-10-16 更正:帮会会话已用全局插入守卫根治 —— `CreateGuild` 与审批通过的第一把锁是 `guild_player_state` 主键 0 那行哨兵 `S(0)`,
`uk_guild` / `uk_guild_member` 的查重插入者都在它上面排成一列,**不再依赖 `guild_id` 单调**(复查见 §9.6.2)。〕

### 9.3 18 条的处置与验证状态

| # | 位置 | 处置 | 真库 |
|---|---|---|---|
| 1 | friend 拉黑写入 | ODKU | ✅ 全套 124s 零 SKIP |
| 2/3/5/6/10 | guild 申请表 / 捐献截止 / 建帮重名 | 主键点锁 + `FORCE INDEX(PRIMARY)`,帮会会话落码 | 归帮会会话〔10-16:已落地,EXPLAIN 回归在;真库新增死锁 0,见 §9.6.1〕 |
| 4/7/8/14/15 | `assetop.AllocateSeq` 未决行加锁读 | 改普通读,对 op 表不再持锁 | ✅〔10-16 HEAD 重验:267 PASS / 0 FAIL / 0 SKIP〕 |
| 9 | `trade_favorite` 收藏 | ODKU(聚簇主键,安全) | ✅ |
| 11 | 快照 guid 去重 | 生成列 + `uk_snapshot_guid_nz`,写入改普通 INSERT + 1062 回查,配去重迁移〔10-16 更正:单行路径实际是 ODKU `id = LAST_INSERT_ID(id)`,1062 只在退路形态当「已存在」〕 | ✅〔审计后新增的批量路径另有 #19 / #20,见 §9.6.3〕 |
| 12 | `player_name` 抢注 | **根治:主键改 `name_norm`、`player_id` 转唯一键、保持 ODKU**,配幂等可回滚迁移 | ✅ |
| 13 | `debug_import` | 事务改 RC + 表遍历定序 | ✅ |
| 16 | seq 行首次创建 | trade:守卫哨兵行 + `EnsureSeqRowTx`;guild 同法自行落 | ✅〔10-16:trade 133 PASS / 0 / 0;guild 以成员行为守卫落地〕 |
| 17 | merge_zone 整区改写 | 按主键升序点更新 + RC + 有界重试;TiDB 悲观短事务由搬迁会话落 | ✅〔10-16:用自起的一次性 MySQL 3306 / Redis 6379 跑通 273 PASS / 0 / 0,含 `TestIT_MySQLSessionIsReadCommitted`、`TestIT_MigrateGuildZone_ConcurrentDisbandDoesNotDeadlock`〕 |
| 18 | gateway 白名单 | ODKU(自增主键,安全) | ✅ 12/12 |

### 9.4 留下的口子

1. 〔2026-10-16:✅ 已关闭 —— 缺口由 `d3c2fca3c`(10-08)补齐,两套在 HEAD `a105c3efd` 上验收模式全绿,见 §9.6.1〕**`go/trade`、`go/shared/assetop` 的真库回归没跑完**:HEAD 编不过(`ledger_dataservice.go` 调的
   `GetPlayerAssetOpLedger` 没有 proto 生成产物,2026-09-29 `ccafe300c` 带入),要先按 §4 重生成。
2. 〔2026-10-16:仍失败,报错相同〕**`TestMigrateSchema_UpgradesLegacyHandWrittenTables` 在干净 HEAD 上就失败**(`transaction_log` 列默认值与
   proto 声明不符),与本批无关,单独处置。
3. **TiDB 一次都没实测**:上面全部结论只对 InnoDB 成立。TiDB 没有间隙锁,现场一/二本来就不存在,
   但 `#11` 的唯一键在 TiDB 上建不出来(不支持 ALTER 加 STORED 生成列、不支持 NULLIF),
   退路那条 `NOT EXISTS` 去重在 TiDB 上**不成立**——这条必须在迁库前解决。
4. **`player_name` 改主键要停写窗口**:迁移是单条 ALTER(手册标注 Permits Concurrent DML=Yes),
   但"旧二进制 + 新表"会让 Release 以 1176 失败,回滚镜像之前必须先回滚 DDL。

### 9.5 教训(补 §8)

5. **手册结论要问"它说的是哪一类现场"。** 同一句 "exclusive lock rather than a shared lock" 被两轮 agent
   分别用来证明"ODKU 能消环"和"ODKU 消不掉",两次都只对一半现场成立。三类现场各有各的成因。
6. **推演能把断言写反。** `#12` 的注释翻过三次、`#11` 的断言翻过两次,每次都有手册引用撑着,
   直到真库一跑才定案。**凡是"某某锁会不会与某某锁互等"的判断,在真库上摆一次交错的成本远低于反复推演。**

## 9.6 两周后复查(2026-10-16)

审计修复随 09-29 起的批量提交入库后,其他会话又在同一批表和服务上改了两周代码。本节核对三件事:修复是否仍在、
真库回归是否仍绿、审计之后新增的加锁 SQL 有没有带进同类问题。代码一律读干净 HEAD `a105c3efd` 的 worktree;
新发现每条交独立复核员反驳,能在真库上摆交错的都摆了(MySQL 26.7.0,全局 RR,`innodb_deadlock_detect=ON`,
`innodb_autoinc_lock_mode=2`)。

### 9.6.1 真库重跑

| 套件 | 结果 | 说明 |
|---|---|---|
| `go/shared/assetop` | 267 PASS / 0 FAIL / 0 SKIP | 验收模式(`ASSETOP_REQUIRE_MYSQL_TESTS=1`) |
| `go/trade/internal/data` | 133 PASS / 0 / 0 | 验收模式,含 #9 / #16 的红对照与绿用例 |
| `go/friend`(MySQL) | 407 PASS / 0 FAIL / 5 SKIP | 4 个是另设门控的 TiDB 用例;1 个是回收竞态用例本轮没撞上窗口(用例自报 SKIP),单独重跑 PASS |
| `go/friend`(TiDB v8.5.2) | 9 PASS | 推荐计划与申请代次的 TiDB 用例 |
| `go/data_service/internal/store` | 365 PASS / 1 FAIL / 1 SKIP | FAIL 是 §9.4-2 那条既有失败;SKIP 要 TiDB。修 #19 之前还多一条 1869(见 §9.6.3) |
| `go/guild`(data + logic) | 〔定案〕logic 690 PASS / 0 / 1 SKIP(要真 Redis);data **339 PASS / 0 FAIL / 0 SKIP**(快实例) | 前几轮的失败**全部**是预算到点的 `context deadline exceeded`,死锁增量始终为 0。定位:本机 Docker 上的 MySQL 每次提交约 **42 ms**(`sync_binlog=1`、`flush_log_at_trx_commit=1`、Windows 虚拟盘),两条经济锁序用例(`TestEconomyLockOrder_LeaveVersusCleanupVersusFinalize` / `…ReserveVersusCleanupSameStream`)每轮要逐行提交 200 次清理,30 轮跑不进 `econLockOrderBudget` 的 120 秒总预算(两次都在 121~122 秒、第 25 / 29 轮断)。换成数据目录在内存盘、`flush_log_at_trx_commit=2`、`sync_binlog=0` 的实例(每次提交 1 ms,**锁语义不变**)后两条各 22~23 秒通过,整包 339 全绿。lock_timeouts 两种实例都是 +7,来自专测 1205 的用例 |
| `java/gateway_node` | 锁序相关全绿 | `ZoneConfigUpsertLockOrderMySqlTest` 2、`ZoneWhitelistUpsertLockOrderMySqlTest` 2、`InnoDbDeadlockRetryTest` 8;另一条 `LoginRpcClientRetryTest` 期望 `UNAVAILABLE` 得到 `DEADLINE_EXCEEDED`,与锁无关 |
| `go/player_inbox` | 未跑成 | HEAD 编不过:`go.sum` 从未入库(模块正被别的会话改,未提交) |
| `tools/merge_zone` | **273 PASS / 0 / 0** | 集成测试写死 `127.0.0.1:3306`、`6379` 与仓库开发口令;临时起了占这两个端口的一次性 MySQL(内存盘)与 Redis,跑完即删(用例碰到非空的 Redis 库会 SKIP 而不是清库) |

### 9.6.2 18 条修复:全部仍在

逐条定位到 HEAD 的代码行,并读了审计后每次改动的 diff:没有一条被改回。值得记一笔的变化:
`#6` 帮会改用全局插入守卫 `S(0)`,不再依赖 `guild_id` 单调(§9.2 已更正);`#11` 的部署前提从
`replicas=1 + Recreate` 变成 2 副本 `RollingUpdate`(`1bc9d4899`),单写者不再成立 —— 这正是 #20 / #21 暴露的原因。

### 9.6.3 审计之后新增代码里的新发现

| # | 位置 | 问题 | 证据 | 处置 |
|---|---|---|---|---|
| 19 | `data_service` `snapshot_store.go` 批量路径(`d5cbb8a8e`,10-13) | 多行 ODKU 尾巴 `id = LAST_INSERT_ID(id)` 给自增列赋值,批内同 guid 重复时 MySQL 报 **1869** 整条回滚 | 真库:`id = LAST_INSERT_ID(id)` 与 `id = id` 都 1869;`snapshot_guid = snapshot_guid` 正确(先出现者落库,affected 只计新插入) | ✅ 已修(随 #20 的重新设计,批量路径不再用 ODKU,1869 一并消失;`4d87229aa` 里的过渡写法 `snapshot_guid = snapshot_guid` 已被取代);`TestSnapshotStore_BatchDedupsAgainstStoredAndWithinBatch` 由红转绿 |
| 20 | 同上 | 多行 ODKU 撞到**已存在**的行时,先在自增主键末尾试插、回滚后留下 **PRIMARY supremum 上的 X 间隙锁直到语句结束**;rebalance 重叠时与另一实例在唯一键上的等待成环 | 两名复核员真库复现:HEAD 尾巴 3/3、#19 修复后的尾巴 3/3、**两批都按 guid 升序也 3/3**;对照「批里没有撞键行」0/3 | ✅ **已修(10-16 重新设计)**:① 普通读按 `uk_snapshot_guid_nz` 预读剔除已存在的 guid 与批内重复;② 剩下的按 **snapshot_guid 升序**拼**普通**多行 INSERT(不带 ODKU / IGNORE);③ 撞 1062(预读之后并发写者落了同一 guid)整条自动回滚、转逐行单行 ODKU。普通 INSERT 撞已提交重复立刻 1062,不会带着间隙锁继续,环的一半被拆掉。真库:新用例 `TestSnapshotStore_BatchOverlapDoesNotDeadlock` 在旧实现上 1213 重跑 1 次(红)、新实现 0 次;复核员手工编排旧 3/3 成环、新 3/3 不成环,3 写者 600 次调用压测新 0 次 1213(旧 59 次);预读 256 参数走 `uk_snapshot_guid_nz` 覆盖索引 range。回退计数 `batchFallbacks` 在常态用例里断言为 0(去掉预读的红对照已确认变红)。残余同单行路径(≥3 写者同 guid 且首插回滚,复核员实测 6/6,由有界重试吸收)。**复核还抓到第二个环**:普通 INSERT 撞未提交重复 x 时,**等待中**的 S next-key 请求也罩住 (pred(x), x),持有 x 的领先写者往这个间隙插后续行会被挡住 —— 按 offset 序拼批 guid 不单调,两写者 [5000, 6000..6149, 4999] 10/10 成环、流式重叠 7/279,所以剔重后改为**按 guid 升序**插(逐行回退同序),同组实验全部 0;新增 `TestSnapshotStore_BatchOutOfGuidOrderOverlapDoesNotDeadlock`(不排序的红对照已确认变红),`BatchIdsFollowRowOrder` 改为 `BatchIdsFollowGuidOrder`(代价:同一玩家同一秒跨节点的快照,列表并列次序随 guid)。`data_service.yaml` 的「建议开服值 16」不再需要撤 |
| 21 | `data_service` `transaction_log_store.go` `InsertBatchIgnore` | 消费者跨分区混拼的一批按到达序写 VALUES;rebalance 重叠时两实例对重叠行顺序相反,主键查重 S,REC_NOT_GAP 互等(RC 下同样成立) | 复核员真库:两条 200 行语句同时放行,到达序全重叠 4/10、半重叠 3/10 成环,**都按 `tx_id` 升序 0/10** | ✅ 已修:拼语句前按 `tx_id` 升序稳定排序(副本,不改调用方切片);`TestTxLogRowsInLockOrder` |
| 22 | `data_service` `route_store.go` 建池探针 P4 + P5 | 空表上 ≥3 个建池方并发插同一个缺席的探针键,先插者(探针必然回滚)回滚后落入 §9.1 现场二,原来只靠 `retryRouteTx` 吸收 | 两名复核员真库(RC)10/10、11/11 成环;加守卫 0/10 | ✅ 已修:探针事务第一句 `route_meta(id=1) FOR UPDATE`(`lockRouteProbeGuard`),有界重试降为最后防线;真库回归 `TestRouteStore_ProbesQueueOnMetaGuard`(去掉守卫的红对照已确认变红)、契约用例「探针守卫缺行」。当前生产 `Mode: redis` 不建 route store,切 db_first 前生效 |
| 23 | `player_inbox` `store.go` `ensureStream` | 流行缺失时无守卫 `INSERT IGNORE`,与 #16 修复前同形(R4);设计文档 §6.2 第 6 步把有界重试当消环论证 | 复核员真库(RC):两个后到者真排在未提交行后 18/18 成环;另一名复核员证当前**不可达**(v1 唯一生产方是运维手调的 GM 处置,逐玩家串行),并更正「至少 3 个并发者才成环」 | **转交**(模块在别的会话手里):第 5 批开放外部生产方之前必须改成「普通读探针 + 守卫行」,并改设计文档论证 |
| 24 | `go/db` `internal/kafka/player_row_fence.go:49`(**在途、未提交**,落地围栏那条线) | 新号首次存盘在全局 RR 事务里先 `FOR UPDATE` 一个不存在的主键(取间隙锁 / supremum next-key),未命中再 `Save`(UPDATE 0 行 → INSERT);两个**不同**新号的首存互挡插入意向锁 → 跨玩家 1213 | 复核员真库(RR):空表与非空表两种形态都按代码语句顺序复现 1213;**当前可达**(建角只写 Redis 种子,每个新号两张表首存都走未命中分支;登录压测每轮清库)。HEAD 的旧路径是自动提交的单条 Save,不受影响 | **已转交**落地围栏会话:未命中分支改为「普通读 → 普通 INSERT,撞 1062 回滚整体重跑」或显式 RC;两种都不许用 Save 的「1062 后按主键 UPDATE」兜底(会绕过围栏);补「不同新号并发首存」真库回归。〔10-16 落地围栏会话已按方案①修:先非锁定 `ExistsByPK`,无行即自动提交 INSERT(不同主键的插入意向锁互相兼容),撞 1062 才进事务对已提交行 `FOR UPDATE` 再按 (epoch, seq) 裁决,事务内仍未命中返回可重试的 `errPlayerRowVanished`;新增 `TestIntegrationPlayerSQLFenceConcurrentFirstSavesOfDifferentPlayersDoNotDeadlock` 等两条回归。**已修;10-16 真库验证 4 条用例全 PASS,死锁 / 锁等待超时计数均为 0**〕 |

补记(#20 复核员,真库实测):**单行 ODKU 会就地吸收任何唯一键上的重复**,不只 `uk_snapshot_guid_nz` —— 表上若多出别的唯一键,撞它的快照会以 affected=0、`LAST_INSERT_ID` 回带别的行 id 的形态被当成「已存在」静默丢弃,原注释「还能撞出 1062 只说明撞的是别的唯一约束」不成立。现网 `player_snapshot` 只有 PRIMARY 与 `uk_snapshot_guid_nz`,无影响;已改正注释,并加守卫 `TestSnapshotTableUniqueIndexesAreOnlyGuidKey`(多出唯一索引即红)。

**未复核、只登记的线索**(额度原因没有派复核员;都不在当前 MySQL 生产路径上):

- **TiDB 语义(5 条,并入 TiDB Phase 1 的 P1-0 待核)**:friend 的 `Unblock`、终态申请清理、容量行回收,player_inbox 的 `ClearNotifyDue`,
  trade 的 `Reschedule` / `markPoison`(代码注释已自认「迁 TiDB 前必须对齐 guild」)——都是「自动提交(TiDB 下走乐观事务)写行 / 改索引列」
  与悲观点锁事务在行 key 与索引 key 上反向互等的推演。处方同 guild `asset_store.go` 的 TiDB 附加规则:改成显式 RC 短事务,首句完整主键 `FOR UPDATE`。
- **在途的交易 commerce(`go/trade/internal/commerce`,未提交)**:`market.go:34` 撮合守卫与 `auction.go:41` 竞拍守卫在同一业务事务里先
  `INSERT IGNORE` 守卫行再 `FOR UPDATE`;复核员在 MySQL RC 下实测 S→X 升级环与首插回滚环都成立(1/1、5/5、3/3),但 commerce 只在
  `AssetOp.Enabled` 时装配,而它被 `config.Validate` 与 `authoritysql.Open` 强制只连 TiDB 悲观事务 —— **当前不可达**,属「放开 TiDB 门禁或照搬到
  MySQL 服务之前必修」的潜伏写法(处方:守卫行启动期预建或在已提交哨兵下独立短事务建,业务事务只留完整主键 `FOR UPDATE`)。`engine.go:377`
  取消交易走二级索引多行改 `trade_asset_op`,绕过「先主键点锁再 CAS」:TiDB 上与 Finalize / ResolveManually 在唯一键上互等(推演,未实测),
  并入 TiDB 待核。
- **在途代码(3 条,转交写 `go/match/internal/authority` 的会话)**:`match_authority_ticket` 与 `match_direct_intent` 在事务内先 `INSERT IGNORE`
  再 `FOR UPDATE`(InnoDB 下 S→X 升级与 R4,`RequireTiDB=false` 时才走到);`Finish` 按成员入队顺序逐条更新票据,与 `claimTx` 的 `player_id` 升序反序。

### 9.6.4 新的实测结论(补 §9.1)

**现场四:多行 ODKU + 自增主键。** 多行语句里只要有一行撞到已存在的记录,该行会先在自增主键末尾试插一条聚簇记录,回滚后那把锁被继承成
PRIMARY supremum 上的 X 间隙锁,一直持有到整条语句结束 —— 于是这条语句挡住全表**所有**并发插入者,而不只是同 guid 的。它与
「本语句稍后在唯一键上等别人的未提交行」组合即成环;排序只能消掉记录锁上的环,消不掉这一个。不相交的两批不成环,但后到者要等前一条
整条语句结束。单行语句不受影响(撞键时它没有「稍后」)。

**1869 ER_AUTO_INCREMENT_CONFLICT。** 多行 INSERT 的 ODKU 子句只要给自增列赋值(哪怕赋回原值),批内重复撞上本语句刚生成的 id 就报错;
撞库里早已存在的行不报。

### 9.6.5 教训(补 §9.5)

7. **共用别人的容器跑真库会被中途撤掉。** 本次复查用的 Codex 会话的 MySQL 在 03:38 收到停机信号,随后整套换成别的演示栈;紧接着的一轮全部
   挂在连接上。真库验收要用**自己起、自己删**的一次性容器(端口错开),并在报告里写明实例。
8. **在负载机上,紧时间预算用例的失败不能当锁序证据。** 判死锁看 InnoDB 计数(`information_schema.INNODB_METRICS` 的 `lock_deadlocks`)
   与 `LATEST DETECTED DEADLOCK`,不看「写冲突」类返回值 —— `inTx` 会把 1213 和预算超时都包成同一个错误。
9. **审计结论有保质期。** #20 / #21 都不是修复被改回,而是部署前提(单副本)在审计之后被别的提交改掉。改副本数、改消费模型的提交,
   要把本文 §9.2 的判据和「多行语句的行序即取锁序」一起过一遍。

## 9.7 TiDB 下的锁问题:真 TiDB 实测与修复(2026-10-18)

§9.6.3 里只登记、没复核的 6 条 TiDB 线索,在自起的真 TiDB v8.5.2 集群(pd + tikv + tidb,全局悲观事务、`pessimistic-auto-commit`
为默认的 false)上逐条复核;会话设置按各服务真实连接照搬(friend 的 `BuildDSN` / trade 的 `authoritysql.Open`:RC、
`innodb_lock_wait_timeout=1`、`tidb_txn_mode=pessimistic`)。**friend 生产强制跑 TiDB**,所以 friend 的几条是生产路径。

### 9.7.1 机制:自动提交 × 点锁,各持一半、检测不到

`pessimistic-auto-commit=false` 时,**自动提交语句走乐观事务**:执行期不加锁,提交时按 region **并行** prewrite 行 key 与
各索引 key。悲观事务里的完整主键 `FOR UPDATE` 是 Point_Get,**先锁 PRIMARY 索引 key、再锁行 key**。两者撞在同一行时
可能各持一半:乐观方已 prewrite 了点锁方要的 key,点锁方已拿到乐观方要 prewrite 的 key。乐观方的 prewrite 遇锁只做
backoff / resolve,不进死锁检测器的等待图 —— **不报 1213,只能等约 3s 的乐观锁 TTL 过期**才解开(`data_lock_waits`
能同时看到两条边,`cluster_deadlocks` 没有记录)。InnoDB 上同样的自动提交单语句「不持有并等待」,不成环 —— 所以这类问题
只在 TiDB 出现,本文 §3~§9.6 的论证都不覆盖它。

另一个形态是**悲观事务里按二级索引的 UPDATE**:执行计划是 `Update → SelectLock → IndexLookUp`,读阶段先锁行 key、
语句末尾才锁非聚簇 PRIMARY 与 uk,与点锁的「先 PRIMARY 再行 key」天然反序;这一种 TiDB **能**检测,报 1213。

### 9.7.2 逐条结论与处置

| 位置 | 实测 | 处置 |
|---|---|---|
| friend `Unblock`(block_repo.go) | **已不成立**:`e76f6f747` 已改为 RC 悲观事务里的单条完整主键 DELETE(悲观事务里它本身就是带锁 Point_Get,顺序与守卫一致);旧的自动提交形状实测约 3s 互等 7 次 | 改正注释:「事务不改变锁形状」只对 InnoDB 成立,TiDB 上正是事务消掉了互等,**不许**改回自动提交 |
| friend 终态申请清理 `deleteTerminalRequestsBefore`(sweep_repo.go) | 成立,检测不到:550 轮 62 轮 3.0~3.9s | ✅ 改为逐行 RC 事务{`lockRequestRowSQL` 主键点锁 → 原带复核的主键 DELETE}(`deleteOneRowPointLocked`);修法实测 800 轮 0 次 |
| friend 容量行回收 `deleteIdleCapacityRow`(sweep_repo.go) | 成立,检测不到:200 轮 12 次、250 轮 13 次 3.0~4.5s | ✅ 同上,点锁用 `lockCapacityRowSQL`;修法实测 0/200、0/250 |
| trade `Reschedule` / `markPoison`(asset_op_repo.go) | 成立,检测不到:自然竞争 31/40 次约 3s,部分轮次撞 5s ReadTimeout | ✅ 改为 RC 事务{`sqlLockAssetOp` 主键点锁 → 原带令牌复核的点改},与 guild 及 `Finalize` 同形;修法 0/40。`Claim` / `ReleaseLease` 只改无索引列,保持自动提交 |
| trade commerce `engine.go` 取消 / 结案三处按 `ref` 的 UPDATE | 成立,**TiDB 能检测**:受控 5/5 报 1213,自然竞态 246 次 10 次 | ✅ 改为 `AssetOpRepo.ExpireCommerceEscrowDebitsTx` / `MarkCommerceOpsConsumedTx`:候选普通读 → 按 op_id 升序逐行主键点锁 → 主键点改;修法 0/5、0/246 |
| player_inbox `ClearNotifyDue` / `DeferNotifyDue` | 成立,检测不到:6 轮 5 轮约 3s,并发的 Fetch 报 1205 | **未改**(文件在收件箱那条线的在途改动里):改为 RC 事务{完整主键 `FOR UPDATE` → 原 CAS UPDATE},修法已实测(E2 提交 10~30ms、无 1205) |

可达性:trade / commerce 只在 `AssetOp.Enabled=true` 时装配,当前所有档位为 false,**开通道前**必须带上上述修复;
friend 的两处清理只在 `Sweep.Mode=delete` 时执行;player_inbox 的形状在入箱 / 补发 / 放弃路径上都会走到。

### 9.7.3 判据(补 §9.2)

**TiDB 上,凡是会被悲观事务点锁的行,对它的写一律在显式悲观事务里、先完整主键点锁再写;不许用自动提交单语句,
也不许在悲观事务里按二级索引批量改。** 只改无索引列(变更集合只有行 key)的自动提交单行写不受影响。
