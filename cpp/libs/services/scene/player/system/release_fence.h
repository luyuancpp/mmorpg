#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>

// scene_manager → scene 的 ReleasePlayer 归属栅栏。设计:docs/design/scene-enter-chain-residuals.md §4(余项 c)。
//
// 为什么需要:ReleasePlayer 原先只带 player_id。scene_manager 的释放有重试(首发失败按 1s / 3s 退避,最坏约 7s 才送达),
// 迟到的那一条会退掉一个仍持有归属的实体:
//   * 交接被 scene_manager 回滚(推路由失败,owner_epoch E → E+2),源节点按回执采纳 E+2 并解冻(B5)之后,迟到的释放到达;
//   * 换到别处后又换回本节点,新载入实体缓存的是更新的一代,迟到的旧释放到达。
// 两种情况下 location 与 gate 会话都还指向本节点,实体被退掉后会话变哑,只能重登。
// 修法:scene_manager 只在本次 EnterScene 提交之后才派发释放,并带上它落点 CAS 用过的观察值 expected_owner_epoch;
// 本节点只在本地缓存恰好等于它时才执行退出这种破坏性动作。
//
// 本头只放纯判定、名表与计数,不含 entt / redis / muduo / protobuf,不读任何时钟(先例:enter_fence.h)。
// 粘合代码在 player_lifecycle.cpp(PlayerLifecycleSystem::HandleReleaseFromSceneManager;会话结束释放走
// HandleSessionEndedRelease,docs/design/location-release-binding.md §3.3),gRPC 入口 scene_node_service.cpp 的
// HandleReleasePlayer 按 session_ended 委托到其中之一。汇总行在 redis.cpp([ReleaseFence])。
// 单测:cpp/tests/cross_zone_test 的 ReleaseFence* / ReleaseFenceEcs*。
//
// 不变量(改任何一条先重过设计 §4.2 与 §4.6):
//   * 只比 owner_epoch(归属代际),不看会话、不看登录代次;登录代次也不进这里的判定(设计 §1.3)。
//   * 只读本地缓存 PlayerOwnerEpochComp.epoch,不读 Redis、不采纳 epoch(CZ-3)。
//   * 判定零副作用,且在任何副作用之前做完:被忽略的请求不碰会话、待入场上下文、改派票据、待确认条目与退出意图,
//     不做 MergeExitCause,也不触发超限重踢。
//   * 「交接在途」先于一切比较:交接已发起的实体去留只由 EnterScene 应答与看门狗裁决(现有守卫,语义不变)。
//     依赖 ResolveTravelOutcome → JudgeTravelOutcomeReply 的 B5(kRolledBackToSelf)采纳「先抬 epoch、后摘交接组件」、
//     两步在同一次回调里同步完成的顺序(player_lifecycle.cpp 该处有注释钉住)。
namespace release_fence
{
	// 判定结论(设计 §4.2 判定表,按顺序判)。前三项释放,后三项忽略。名字同时是 [ReleaseFence] 汇总行里的 key,
	// 改名要同步运维检索。底层类型 uint8_t 数的是结论种类数(现 6 种)。kCount 固定为最后一项,同时是名表长度。
	enum class Verdict : uint8_t
	{
		kRelease,             // 缓存 == 期望:释放(现行为)
		kReleaseUnversioned,  // 期望 0(旧版 scene_manager 或尚未铸造):按旧行为释放,计数;scene_manager 全量升级后应恒为 0
		kReleaseCacheUnknown, // 缓存 0(含组件缺失):按旧行为释放,计数并 WARN。不选 fail-closed:缓存为 0 的实体存盘不带
		                      // guard,留着它反而可能用旧状态覆盖 Redis 与 MySQL
		kSkipHandoffInFlight, // 交接已发起:忽略,去留由 EnterScene 应答与看门狗裁决(现有守卫,语义不变)
		kSkipStaleRelease,    // 缓存 > 期望:实体已采纳回滚(E+2)或已在更新的一代重新载入,这是一条迟到的旧释放
		kSkipAheadOfHolder,   // 缓存 < 期望(两者都非 0):生产上构造不出(标记 epoch 就是缓存值,缓存只增不减),只在
		                      // dev 旁路或键异常时出现。退出是破坏性动作,要求严格相等;这样的陈旧实体下一次带 guard 的存盘
		                      // 会被 CAS 拒掉、按废黜流程自毁,不会写坏数据

		kCount
	};

	constexpr std::size_t kVerdictCount = static_cast<std::size_t>(Verdict::kCount);

	inline constexpr const char *kVerdictNames[] = {
		"release",
		"release_unversioned",
		"release_cache_unknown",
		"skip_handoff_in_flight",
		"skip_stale_release",
		"skip_ahead_of_holder",
	};
	static_assert(std::size(kVerdictNames) == kVerdictCount, "kVerdictNames 必须与 Verdict 一一对应");

	constexpr const char *VerdictName(Verdict verdict)
	{
		const auto index = static_cast<std::size_t>(verdict);
		return index < kVerdictCount ? kVerdictNames[index] : "?";
	}

	// 是否执行退出。越界值按不释放处理(fail-closed:退出是破坏性动作)。
	constexpr bool Releases(Verdict verdict)
	{
		switch (verdict)
		{
		case Verdict::kRelease:
		case Verdict::kReleaseUnversioned:
		case Verdict::kReleaseCacheUnknown:
			return true;
		default:
			return false;
		}
	}

	// 出现即值得人看一眼的结论:30s 汇总行在这几项有增长时升 WARN(设计 §4.4 第 4 条)。
	// skip_handoff_in_flight 是生产常态(成功交接时释放到达,源实体多半仍在交接中),release_unversioned 是滚动窗口常态,都不升级。
	constexpr bool Alarming(Verdict verdict)
	{
		switch (verdict)
		{
		case Verdict::kReleaseCacheUnknown:
		case Verdict::kSkipStaleRelease:
		case Verdict::kSkipAheadOfHolder:
			return true;
		default:
			return false;
		}
	}

	// 一条 ReleasePlayer 对本地实体的结论(设计 §4.2,按顺序判):
	//   handoffRequested                  → kSkipHandoffInFlight
	//   expectedOwnerEpoch == 0           → kReleaseUnversioned
	//   cachedOwnerEpoch == 0             → kReleaseCacheUnknown
	//   cached == expected                → kRelease
	//   cached >  expected                → kSkipStaleRelease
	//   cached <  expected                → kSkipAheadOfHolder
	// expectedOwnerEpoch 是 scene_manager 本次 EnterScene 观察到、并用于落点 CAS 的值(ReleasePlayerRequest.expected_owner_epoch);
	// cachedOwnerEpoch 是本地 PlayerOwnerEpochComp.epoch,组件缺失按 0 传入;handoffRequested 即 IsHandoffRequested。
	constexpr Verdict Decide(uint64_t expectedOwnerEpoch, uint64_t cachedOwnerEpoch, bool handoffRequested)
	{
		if (handoffRequested)
		{
			return Verdict::kSkipHandoffInFlight;
		}
		if (expectedOwnerEpoch == 0)
		{
			return Verdict::kReleaseUnversioned;
		}
		if (cachedOwnerEpoch == 0)
		{
			return Verdict::kReleaseCacheUnknown;
		}
		if (cachedOwnerEpoch == expectedOwnerEpoch)
		{
			return Verdict::kRelease;
		}
		return cachedOwnerEpoch > expectedOwnerEpoch ? Verdict::kSkipStaleRelease : Verdict::kSkipAheadOfHolder;
	}

	// ── 会话结束释放(ReleasePlayerRequest.session_ended,docs/design/location-release-binding.md §3.3)──────────────
	//
	// scene_manager 的 LeaveScene 拿不到"持有者已释放"证据时请 location 的持有节点释放。本节点有该玩家实体时,判定与交接释放
	// 共用上面的 Decide(释放 = 按客户端断线退出,存盘落地后由 A1′ 写标记);本节点**完全不持有**时,按下面的判定决定要不要按
	// owner_epoch == expected 条件补写一枚凭证标记,让 scene_manager 的下一次重试拿到证据删 location。
	// 安全性与"改派踢线前的凭证补写"(relocate_confirm::DecideCredential)同理:本节点没有该玩家的任何内存态,盘上就是最终态;
	// 条件写保证不会给更新的一代发证。

	// 凭证判定结论。除 kWrite 外每一项都是一个"不写"的原因。名字同时是 [ReleaseFence] 汇总行里 credential_ 后缀的 key。
	// 底层类型 uint8_t 数的是结论种类数(现 6 种)。kCount 固定为最后一项,同时是名表长度。
	enum class SessionEndedCredential : uint8_t
	{
		kWrite,
		kSkipUnversioned,  // 期望 0:scene_manager 不为 epoch 0 发会话结束释放;防御写法,写了也是"0:t",换手门不认
		kSkipLocalHolder,  // 本节点有实体句柄 / 在途载入 / 改派待确认条目:不是"完全不持有",由那些流程自己收尾
		kSkipMarkDisabled, // A1′ 开关关闭(SCENE_EXIT_RELEASE_MARK=0):回滚期间不发任何新标记(exit_release_mark.h)
		kSkipDevBypass,    // dev 旁路开着(SCENE_DEV_UNSAFE_CROSS_NODE_HANDOFF,与 scene_manager 的旁路同步)
		kSkipIdentity,     // 本节点身份不确认(疏散中 / etcd 租约推定过期,M3):写标记可能给接手的新进程开放行口

		kCount
	};

	constexpr std::size_t kSessionEndedCredentialCount = static_cast<std::size_t>(SessionEndedCredential::kCount);

	inline constexpr const char *kSessionEndedCredentialNames[] = {
		"write",
		"skip_unversioned",
		"skip_local_holder",
		"skip_mark_disabled",
		"skip_dev_bypass",
		"skip_identity",
	};
	static_assert(std::size(kSessionEndedCredentialNames) == kSessionEndedCredentialCount,
				  "kSessionEndedCredentialNames 必须与 SessionEndedCredential 一一对应");

	constexpr const char *SessionEndedCredentialName(SessionEndedCredential decision)
	{
		const auto index = static_cast<std::size_t>(decision);
		return index < kSessionEndedCredentialCount ? kSessionEndedCredentialNames[index] : "?";
	}

	// 凭证判定的输入。默认值全部取"不写"的一侧(fail-closed):漏填任何一项都只会少写。
	struct SessionEndedFacts
	{
		uint64_t expectedOwnerEpoch{0};  // ReleasePlayerRequest.expected_owner_epoch
		bool localHolderPossible{true};  // playerList 有该玩家(含无效句柄)/ 有待入场条目 / 有改派待确认条目
		bool markFeatureEnabled{false};  // A1′ 开关(SCENE_EXIT_RELEASE_MARK)
		bool devUnsafeCrossNode{true};   // dev 旁路开关(SCENE_DEV_UNSAFE_CROSS_NODE_HANDOFF)
		bool identityConfirmed{false};   // 本节点身份当前确认有效
	};

	// 全部满足才写;第一个不满足的条件就是计数的原因。顺序只影响"计到哪个原因",不影响写不写。
	constexpr SessionEndedCredential DecideSessionEndedCredential(const SessionEndedFacts &facts)
	{
		if (facts.expectedOwnerEpoch == 0)
		{
			return SessionEndedCredential::kSkipUnversioned;
		}
		if (facts.localHolderPossible)
		{
			return SessionEndedCredential::kSkipLocalHolder;
		}
		if (!facts.markFeatureEnabled)
		{
			return SessionEndedCredential::kSkipMarkDisabled;
		}
		if (facts.devUnsafeCrossNode)
		{
			return SessionEndedCredential::kSkipDevBypass;
		}
		if (!facts.identityConfirmed)
		{
			return SessionEndedCredential::kSkipIdentity;
		}
		return SessionEndedCredential::kWrite;
	}
} // namespace release_fence

// 栅栏计数。写法同 enter_fence.h 的 enter_fence_stats(relaxed atomic、不接 Prometheus、player_id 不做 label、
// 只在 scene 逻辑线程上写),由 RedisSystem 的 30s 快照定时器在计数有变化时打成一行 [ReleaseFence](进程启动以来的累计值;
// release_fence::Alarming 的几项有增长时 WARN,否则 INFO)。
//
//   verdicts[]   HandleReleaseFromSceneManager / HandleSessionEndedRelease 对本地实体的每一次判定按结论计数,
//                key 见 release_fence::kVerdictNames
//   not_found    交接释放时玩家不在本节点(幂等返回)。成功交接后源实体已销毁时就是这一项,数值大不代表异常
//   session_ended            收到的会话结束释放总数(docs/design/location-release-binding.md §3.3)。登出 / 断线后
//                            LeaveScene 拿不到释放证据时才会发,平时低速率;持续升高看 scene_manager 的 pending 计数
//   credential_<结论>        本节点完全不持有时的凭证判定分布,key 见 release_fence::kSessionEndedCredentialNames
//   credential_written / credential_epoch_moved / credential_failed  凭证条件写的结果(与 A1′ 同一段 Lua)
namespace release_fence_stats
{
	struct Counters
	{
		std::array<std::atomic<uint64_t>, release_fence::kVerdictCount> verdicts{};
		std::atomic<uint64_t> notFound{0};
		std::atomic<uint64_t> sessionEnded{0};
		std::array<std::atomic<uint64_t>, release_fence::kSessionEndedCredentialCount> credentialDecisions{};
		std::atomic<uint64_t> credentialWritten{0};
		std::atomic<uint64_t> credentialEpochMoved{0};
		std::atomic<uint64_t> credentialFailed{0};
	};

	inline Counters &Get()
	{
		static Counters g_counters;
		return g_counters;
	}

	inline void Inc(std::atomic<uint64_t> &counter) { counter.fetch_add(1, std::memory_order_relaxed); }

	// 越界值不计(Decide 产不出越界值;防御写法,不读数组外)。
	inline void IncVerdict(release_fence::Verdict verdict)
	{
		const auto index = static_cast<std::size_t>(verdict);
		if (index < release_fence::kVerdictCount)
		{
			Inc(Get().verdicts[index]);
		}
	}

	// 越界值不计(DecideSessionEndedCredential 产不出越界值;防御写法)。
	inline void IncCredentialDecision(release_fence::SessionEndedCredential decision)
	{
		const auto index = static_cast<std::size_t>(decision);
		if (index < release_fence::kSessionEndedCredentialCount)
		{
			Inc(Get().credentialDecisions[index]);
		}
	}

	struct Snapshot
	{
		std::array<uint64_t, release_fence::kVerdictCount> verdicts{};
		uint64_t notFound{0};
		uint64_t sessionEnded{0};
		std::array<uint64_t, release_fence::kSessionEndedCredentialCount> credentialDecisions{};
		uint64_t credentialWritten{0};
		uint64_t credentialEpochMoved{0};
		uint64_t credentialFailed{0};

		bool operator==(const Snapshot &) const = default;

		// 与上一次汇总相比,release_fence::Alarming 的几项是否有增长:30s 汇总行据此判断要不要升 WARN。
		bool AlarmingIncreaseSince(const Snapshot &previous) const
		{
			for (std::size_t i = 0; i < verdicts.size(); ++i)
			{
				if (release_fence::Alarming(static_cast<release_fence::Verdict>(i)) && verdicts[i] > previous.verdicts[i])
				{
					return true;
				}
			}
			return false;
		}
	};

	inline Snapshot Read()
	{
		const auto &counters = Get();
		Snapshot snapshot;
		for (std::size_t i = 0; i < snapshot.verdicts.size(); ++i)
		{
			snapshot.verdicts[i] = counters.verdicts[i].load(std::memory_order_relaxed);
		}
		snapshot.notFound = counters.notFound.load(std::memory_order_relaxed);
		snapshot.sessionEnded = counters.sessionEnded.load(std::memory_order_relaxed);
		for (std::size_t i = 0; i < snapshot.credentialDecisions.size(); ++i)
		{
			snapshot.credentialDecisions[i] = counters.credentialDecisions[i].load(std::memory_order_relaxed);
		}
		snapshot.credentialWritten = counters.credentialWritten.load(std::memory_order_relaxed);
		snapshot.credentialEpochMoved = counters.credentialEpochMoved.load(std::memory_order_relaxed);
		snapshot.credentialFailed = counters.credentialFailed.load(std::memory_order_relaxed);
		return snapshot;
	}
} // namespace release_fence_stats
