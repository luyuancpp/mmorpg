#pragma once

// 战斗结算**发件箱**的纯判定逻辑(docs/design/routing-identity-audit-20260908.md R07)。
//
// 缺陷原状:battle 结算时向 `SceneCommand{target_instance_id=CreateBattle 时抓的 scene uuid}`
// 发一次就不管了。那个 scene 进程如果在战斗期间重启,继任者按代次校验丢弃命令并提交 offset,
// **没有任何重投**;而"待结算"的 Redis 记录一直以来是由 *scene* 写的
// (player_battle.cpp ApplySettlement 的离线分支)—— 那个 scene 从来没跑过这段代码。
// 净结果:奖励永久丢失,玩家冻结到 deadline+60s 被 reaper 解开。
//
// 修法(审计推荐的 ①「落库再投递」):
//   1. battle 先把结算写进跨进程存储(shared Redis 的 battle:settlement:pending:{player_id}
//      + 伴生 battle:settlement:pending:id:{player_id} = battle_id),**写成功之后**才投递命令。
//      这条记录本来就有消费者:scene 的 OnPlayerEnterScene 登录钩子会读它并补应用。
//      也就是说即使投递整条丢失,奖励最终仍会在玩家下次进场景时落地。
//   2. scene 应用结算后**条件销账**(按 battle_id 匹配才删),销账即 ACK。
//   3. 未销账的由 battle 有界重投,重投前**重新解析**目标(player:{id}:location),
//      不复用开局时抓的那个 scene uuid —— 那个 uuid 恰恰是过期的那一个。
//   4. 次数用尽响一声(结构化 metric),记录留给登录补应用兜底。
//
// 2026-10-14 起待结算记录**不再过期**(docs/design/landing-recovery-sync-digest.md §7,不变量 L-6):
// 原来的 7 天 TTL 到期由 Redis 静默删除,奖励丢失且没有任何信号。现在记录一直保留到应用并销账为止,
// 另有积压索引 kPendingSettlementIndexKey 让 battle 节点定期报积压数与最老年龄。
//
// 这个头只放**判定与契约**:纯函数 + Redis key / Lua 常量、零项目依赖,能被单测直接盯住
// (与 node_kafka_command_filter.h 拆出来的理由一样 —— 否则要测就得把 Redis、Kafka、muduo loop
// 整条链拖进测试工程)。所有读写待结算记录的 Lua 都集中在这里:「写入必 ZADD 索引、销账必 ZREM」
// 这条规则只有放在一处才审得过来。

#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace battle_settlement
{
	enum class RetryAction
	{
		// 持久记录已不指向本局(scene 销账了,或玩家已经进了下一场):投递成功,摘除条目。
		kDone,
		// 记录仍在、且解析出了玩家当前所在的 scene node_id:按**重新解析出的**目标重投。
		kResend,
		// 记录仍在,但解析不到玩家当前位置(离线 / 还没落进任何场景):
		// 本轮不投。硬要投也没有目标,而持久记录会被登录钩子兜住。
		kSkipNoTarget,
		// 重试次数用尽:摘除条目并打一条**响亮**的结构化日志。
		// 注意这不等于结算丢失 —— 持久记录仍在(不再过期),玩家下次进场景会补应用;
		// 在线玩家战斗冻结到期时 scene reaper 也会补读一次。
		kExhausted,
	};

	struct RetryInput
	{
		// battle:settlement:pending:id:{player_id} 读回来是否仍等于本局 battle_id。
		// false 有两种来源,对 battle 来说是同一件事(不必再投):scene 已销账,
		// 或者玩家已经打完了下一场并被那一场覆盖。
		bool pendingRecordStillOurs = false;
		// player:{id}:location 是否解析出了一个可用(非 0)的 scene node_id。
		bool locationResolved = false;
		// 已经消耗掉的重投轮次(本轮之前)。
		uint32_t attemptsSoFar = 0;
		uint32_t maxAttempts = 0;
	};

	// 判定顺序不能换:
	//   销账 > 次数用尽 > 无目标 > 重投。
	// 「销账」必须排在「次数用尽」之前 —— 最后一轮恰好读到已销账时应当算成功收尾,
	// 而不是打一条"未送达"的假警报。
	inline RetryAction ClassifyRetry(const RetryInput &input)
	{
		if (!input.pendingRecordStillOurs)
		{
			return RetryAction::kDone;
		}
		if (input.attemptsSoFar >= input.maxAttempts)
		{
			return RetryAction::kExhausted;
		}
		if (!input.locationResolved)
		{
			return RetryAction::kSkipNoTarget;
		}
		return RetryAction::kResend;
	}

	inline const char *RetryActionName(const RetryAction action)
	{
		switch (action)
		{
		case RetryAction::kDone:
			return "done";
		case RetryAction::kResend:
			return "resend";
		case RetryAction::kSkipNoTarget:
			return "skip_no_target";
		case RetryAction::kExhausted:
			return "exhausted";
		}
		return "unknown";
	}

	// 次数用尽时的分类(metric=battle_settlement_undelivered 的 reason 字段),便于分开设阈值:
	//   kNoTarget —— 最后一轮解析不到玩家位置(离线 / 还没落进任何场景)。离线玩家 120 秒后
	//                必然走到这里,是预期内的;记录留给登录钩子。
	//   kNotAcked —— 最后一轮解析得到位置、也重投了,scene 仍没销账。玩家在线却收不到奖励,是异常。
	// 只看**最后一轮**而不看「窗口内是否有过目标」:玩家打完立刻下线时,早期几轮还能读到退出中的位置
	// (scene 视同离线、只暂存不销账),按「有过目标」会把这类预期内的情形误报成 not_acked。
	enum class UndeliveredReason
	{
		kNoTarget,
		kNotAcked,
	};

	inline UndeliveredReason ClassifyUndelivered(const RetryInput &exhaustedRound)
	{
		return exhaustedRound.locationResolved ? UndeliveredReason::kNotAcked : UndeliveredReason::kNoTarget;
	}

	inline const char *UndeliveredReasonName(const UndeliveredReason reason)
	{
		switch (reason)
		{
		case UndeliveredReason::kNoTarget:
			return "no_target";
		case UndeliveredReason::kNotAcked:
			return "not_acked";
		}
		return "unknown";
	}

	// ---- 积压报数的纯计算 ----

	// 最老一条待结算记录的年龄(秒)。oldestEnqueuedAtMs 是索引里的分数(写入方的 Unix 毫秒);
	// 0 = 索引为空。分数晚于 nowMs(写入方与报数方墙钟有偏差)时按 0 处理,不报负数也不回绕。
	inline uint64_t PendingBacklogOldestAgeSec(const uint64_t oldestEnqueuedAtMs, const uint64_t nowMs)
	{
		if (oldestEnqueuedAtMs == 0 || oldestEnqueuedAtMs >= nowMs)
		{
			return 0;
		}
		return (nowMs - oldestEnqueuedAtMs) / 1000;
	}

	// 孤儿清理的扫描窗口游标(按排名,0 = 最老)。
	//
	// 设计 §7 原文是「每轮查最老的 10 条」。只查头部有一个会卡死的情形:记录不再过期,打完就再也不上线的
	// 玩家会让有效记录永久占住索引头部;头部一旦有 ≥ 一批有效记录,排在后面的孤儿就永远查不到,
	// count 永久偏大。所以两种窗口交替:
	//   偶数轮 —— 查头部 [0, batch):头部孤儿最多两轮就清掉,oldest_age_s 不会被它们抬高(设计的原意);
	//   奇数轮 —— 查轮转窗口 [rotatingStart, rotatingStart + batch):逐轮后移,扫到底绕回 0,
	//             保证每个孤儿最终都会被查到。清理会让后面的排名前移,一趟里可能跳过几条,下一趟补上。
	// 游标只在 ZRANGE 的回复到达时推进(AdvancePendingIndexSweep):回复没来,下一轮重查同一个窗口。
	struct PendingIndexSweepCursor
	{
		uint64_t round = 0;
		uint64_t rotatingStart = 0;
	};

	// 本轮窗口的起始排名。
	inline uint64_t PendingIndexSweepStart(const PendingIndexSweepCursor &cursor)
	{
		return cursor.round % 2 == 0 ? 0 : cursor.rotatingStart;
	}

	// 本轮 ZRANGE 返回 returned 个成员之后推进游标。奇数轮满批则轮转窗口后移一批,不满(扫到底)则绕回 0。
	// batch 为 0 是调用方缺陷:游标不动,避免原地空转把轮转窗口钉死在一处。
	inline void AdvancePendingIndexSweep(PendingIndexSweepCursor &cursor, const uint64_t returned,
										 const uint32_t batch)
	{
		if (batch == 0)
		{
			return;
		}
		if (cursor.round % 2 != 0)
		{
			cursor.rotatingStart = returned < batch ? 0 : cursor.rotatingStart + batch;
		}
		++cursor.round;
	}

	// 解析积压索引的成员(十进制 player_id)。整串都是十进制且 > 0 才返回 true;
	// 空串、符号、空白、溢出一律 false。成员只由本头的写入脚本以 %llu 写入,解析失败说明索引被手工改过。
	inline bool ParsePendingIndexMember(const std::string_view member, uint64_t &playerId)
	{
		if (member.empty())
		{
			return false;
		}
		uint64_t parsed = 0;
		const char *end = member.data() + member.size();
		const auto [ptr, ec] = std::from_chars(member.data(), end, parsed, 10);
		if (ec != std::errc{} || ptr != end || parsed == 0)
		{
			return false;
		}
		playerId = parsed;
		return true;
	}

	// ---- Redis key 契约(battle 写、scene 读与销账,两端都引用本头,不得各抄一份)----
	//
	// 值 = 序列化的 BattleSettlementEvent;伴生 id 键 = 十进制 battle_id。
	// 两个键**同 SET / 同 DEL**,与既有的 battle:lock + battle:ctx 同一形态:
	// 单靠 blob 无法在 Lua 里判断它属于哪一局(Lua 不会解 protobuf),伴生 id 键就是
	// 「条件销账」与「ACK 探测」共同的那把尺子。
	// 两键都**不带 TTL**(landing-recovery §7):SET 不带 EX 会顺带清掉旧版本写入时留下的 TTL。
	// 每玩家单槽:同一时间每个玩家最多一局待结算,靠战斗锁一直保留到落盘销账来保证,下一局的写入覆盖上一局。
	// 内存上界 = 有待结算的玩家数 × 一份结算 blob。
	inline constexpr char kPendingSettlementKeyFmt[] = "battle:settlement:pending:%llu";
	inline constexpr char kPendingSettlementIdKeyFmt[] = "battle:settlement:pending:id:%llu";

	// 积压索引:ZSET,member = 十进制 player_id,score = 写入时刻(写入方 Unix 毫秒)。
	// 只用于报数(积压条数、最老年龄),不参与任何投递或销账判定 —— 索引丢了、多了、少了都不影响结算正确性。
	//
	// 滚动升级兼容(设计 §7):
	//   - 旧版 battle / scene 写入仍带 7 天 TTL、且不 ZADD:这批记录不进索引,7 天后照旧过期,行为与改造前相同;
	//   - 新版写入不过期、且 ZADD;旧版 scene 暂存时会给它重新挂上 TTL,到期后索引里留下孤儿;
	//   - 旧版 scene 销账不 ZREM,索引里也会留下孤儿。
	// 孤儿统一由 battle 节点的报数任务按 kSweepPendingIndexOrphanScript 清理(每轮查一批,窗口见
	// PendingIndexSweepCursor)。孤儿只让报数偏大,不影响任何结算。
	// 不与上面两个按玩家的键共用前缀格式:"index" 不是十进制,不会被 %llu 的键撞上。
	inline constexpr char kPendingSettlementIndexKey[] = "battle:settlement:pending:index";

	// battle 落库(两键不过期 + 进积压索引),一条 Lua 原子完成。
	// KEYS[1]=blob 键,KEYS[2]=id 键,KEYS[3]=积压索引;
	// ARGV[1]=blob,ARGV[2]=battle_id,ARGV[3]=now_ms(写入方 Unix 毫秒),ARGV[4]=player_id。
	// now 由调用方传入而不是在 Lua 里取 TIME:脚本保持确定性;各节点墙钟偏差是秒级,对以天计的年龄报数无影响。
	// ZADD 不带 NX:同一玩家再写一次(下一局覆盖单槽)就是一条新记录,年龄理应从这一刻重新算。
	inline constexpr char kSetPendingSettlementScript[] =
		"redis.call('SET', KEYS[1], ARGV[1]); "
		"redis.call('SET', KEYS[2], ARGV[2]); "
		"redis.call('ZADD', KEYS[3], ARGV[3], ARGV[4]); return 1";

	// scene 离线暂存:锁值 == battle_id 才写 pending 两键并进积压索引,一条 Lua 原子完成。
	// KEYS[1]=battle:lock,KEYS[2]=pending blob,KEYS[3]=pending:id,KEYS[4]=积压索引;
	// ARGV[1]=battle_id,ARGV[2]=blob,ARGV[3]=now_ms,ARGV[4]=player_id。
	// 返回 0 = 锁不在,1 = 已写入,2 = 锁属于别的局。
	// 为什么不能「先 GET 锁、回调里再 SET」:锁在结算应用后一直留到落盘销账,所以「看到锁 == 本局」
	// 不再证明本局还没应用。两步之间若插进一次销账(同一条 Lua 删 pending + 锁,可能来自另一个节点),
	// 随后的 SET 会把已销账的 pending 重新写回来 —— 销账回调随即从账本摘掉这一局,下次登录就会再发一次奖
	// (第二轮评审 w0zw729x1 #0)。合成一条脚本后,销账之后到达的暂存必然看到「锁不在」,什么都不写。
	// battle 已经写过同一份,这里是幂等覆盖;ZADD 会把年龄刷新到暂存时刻(通常在发件箱重投窗口 120 秒以内)。
	inline constexpr char kStorePendingSettlementIfLockMatchScript[] =
		"local v = redis.call('GET', KEYS[1]) "
		"if not v then return 0 end "
		"if v ~= ARGV[1] then return 2 end "
		"redis.call('SET', KEYS[2], ARGV[2]) "
		"redis.call('SET', KEYS[3], ARGV[1]) "
		"redis.call('ZADD', KEYS[4], ARGV[3], ARGV[4]) "
		"return 1";

	// scene 落盘确认后的销账 + 放锁,一条 Lua 原子完成(= 给 battle 侧发件箱的 ACK)。
	// KEYS[1]=pending blob,KEYS[2]=pending:id,KEYS[3]=battle:lock,KEYS[4]=battle:ctx,KEYS[5]=积压索引;
	// ARGV[1]=battle_id,ARGV[2]=player_id。
	// pending:id == battle_id 才删 pending 两键并 ZREM 索引;lock == battle_id 才删 lock + ctx。
	// 返回位掩码:1 = 删了 pending,2 = 删了锁。
	// 必须**条件删**:迟到的结算不能把玩家**下一场**的待结算记录 / 索引成员删掉(与 battle:lock 的条件删
	// 同一条纪律)。pending:id 不匹配时不 ZREM —— 那时索引成员代表的是下一局的记录。
	// 两件事必须在同一条脚本里:锁是「这一局还没完」的唯一跨进程证据,分两条命令时若只落地了删锁,
	// 玩家就能开下一场,而 battle 仍在重投这一局、下一局的无条件 SET 还会覆盖掉这一局的 pending。
	inline constexpr char kAckSettlementScript[] =
		"local r = 0 "
		"if redis.call('GET', KEYS[2]) == ARGV[1] then "
		"redis.call('DEL', KEYS[1]); redis.call('DEL', KEYS[2]); "
		"redis.call('ZREM', KEYS[5], ARGV[2]); r = 1 end "
		"if redis.call('GET', KEYS[3]) == ARGV[1] then "
		"redis.call('DEL', KEYS[4]); redis.call('DEL', KEYS[3]); r = r + 2 end "
		"return r";

	// scene 登录钩子读到解不开的 blob:无从得知 battle_id,无法条件删,两键与索引成员一起无条件删。
	// KEYS[1]=pending blob,KEYS[2]=pending:id,KEYS[3]=积压索引;ARGV[1]=player_id。
	// 伴生 id 键必须一起删,否则它会永远指向一个再也读不出内容的记录,battle 侧发件箱据此判定
	// "未销账"而重投到次数用尽;索引成员一起删,否则就是一个只能靠报数任务清理的孤儿。
	inline constexpr char kDeleteCorruptPendingSettlementScript[] =
		"redis.call('DEL', KEYS[1], KEYS[2]); "
		"redis.call('ZREM', KEYS[3], ARGV[1]); return 1";

	// battle 报数任务的孤儿清理:pending:id 键不存在才 ZREM 该玩家。
	// KEYS[1]=积压索引,KEYS[2]=该玩家的 pending:id 键;ARGV[1]=player_id。返回 1 = 删了孤儿,0 = 记录仍在。
	// EXISTS 与 ZREM 必须在同一条 Lua 里:分两条命令时,两步之间若恰好写入了新一局(SET + ZADD),
	// 随后的 ZREM 会把一条有效记录从索引里摘掉,它要等下一次写入才会重新进索引。
	inline constexpr char kSweepPendingIndexOrphanScript[] =
		"if redis.call('EXISTS', KEYS[2]) == 0 then "
		"return redis.call('ZREM', KEYS[1], ARGV[1]) end return 0";

	// battle 报数任务的读数:返回 {ZCARD, 最老一条的分数(毫秒,索引为空时 0)}。只读。
	// KEYS[1]=积压索引。分数按 tonumber 转成整数应答(毫秒时间戳在 double 内精确)。
	inline constexpr char kReadPendingBacklogScript[] =
		"local oldest = redis.call('ZRANGE', KEYS[1], 0, 0, 'WITHSCORES') "
		"local oldestMs = 0 "
		"if oldest[2] then oldestMs = tonumber(oldest[2]) end "
		"return {redis.call('ZCARD', KEYS[1]), oldestMs}";
} // namespace battle_settlement
