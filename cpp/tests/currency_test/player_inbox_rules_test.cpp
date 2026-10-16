// 玩家持久收件箱消费端纯规则的用例(docs/design/player-inbox-service.md §4.4、§7.3、§7.6、§7.7、§8 #19)。
//
// 被测对象 player/system/player_inbox_rules.h 零 ECS / 零全局状态 / 零时钟:这里只喂 proto 与整数,
// 验可观察的返回值与账本内容,不碰实体、不连 Redis、不发 gRPC(AGENTS §11.4 确定性)。

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <string>

#include "player/system/player_inbox_rules.h"
#include "proto/common/component/player_inbox_ledger_comp.pb.h"
#include "proto/player_inbox/player_inbox.pb.h"

namespace
{
	namespace rules = player_inbox_rules;

	constexpr uint32_t kGmDomain = player_inbox::PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION;
	constexpr uint32_t kBlockType = player_inbox::PLAYER_INBOX_MSG_GM_BLOCK_CURRENCY_GAIN;
	constexpr uint32_t kDebtType = player_inbox::PLAYER_INBOX_MSG_GM_ATTACH_CURRENCY_DEBT;

	constexpr uint32_t Outcome(player_inbox::PlayerInboxOutcome outcome)
	{
		return static_cast<uint32_t>(outcome);
	}

	PlayerInboxDecision MakeDecision(uint64_t seq, player_inbox::PlayerInboxOutcome outcome, uint64_t requestId,
									 uint32_t msgType = kDebtType, uint32_t reason = 0)
	{
		PlayerInboxDecision decision;
		decision.set_seq(seq);
		decision.set_outcome(Outcome(outcome));
		decision.set_reason(reason);
		decision.set_request_id(requestId);
		decision.set_msg_type(msgType);
		return decision;
	}

	// 记一串 APPLIED,request_id = seq * 10,便于断言。
	void RecordApplied(PlayerInboxLedgerComp &ledger, uint64_t fromSeq, uint64_t toSeq)
	{
		auto &cursor = rules::MutableCursor(ledger, kGmDomain);
		for (uint64_t seq = fromSeq; seq <= toSeq; ++seq)
		{
			ASSERT_TRUE(rules::RecordDecision(cursor, MakeDecision(seq, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, seq * 10)))
				<< "seq=" << seq;
		}
	}
} // namespace

// ── 记账与最近结局环 ─────────────────────────────────────────────────────────

TEST(PlayerInboxRules, RecordAdvancesWatermarkAndAppendsInSeqOrder)
{
	PlayerInboxLedgerComp ledger;
	auto &cursor = rules::MutableCursor(ledger, kGmDomain);
	EXPECT_EQ(0u, cursor.decided_through_seq());

	// seq 允许有空洞:被跳过的 seq 只可能是从未投给本消费方的终态行(设计 §4.4)。
	EXPECT_TRUE(rules::RecordDecision(cursor, MakeDecision(5, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 50)));
	EXPECT_TRUE(rules::RecordDecision(
		cursor, MakeDecision(9, player_inbox::PLAYER_INBOX_OUTCOME_REJECTED, 90, kDebtType, 29003)));
	EXPECT_TRUE(rules::RecordDecision(cursor, MakeDecision(12, player_inbox::PLAYER_INBOX_OUTCOME_SKIPPED, 120, kBlockType, 1)));

	EXPECT_EQ(12u, cursor.decided_through_seq());
	ASSERT_EQ(3, cursor.recent_size());
	EXPECT_EQ(5u, cursor.recent(0).seq());
	EXPECT_EQ(9u, cursor.recent(1).seq());
	EXPECT_EQ(29003u, cursor.recent(1).reason());
	EXPECT_EQ(12u, cursor.recent(2).seq());
	EXPECT_TRUE(rules::ValidateLedger(ledger).empty());
}

TEST(PlayerInboxRules, RecordRefusesStaleSeqAndUnrecordableOutcomeWithoutSideEffects)
{
	PlayerInboxLedgerComp ledger;
	auto &cursor = rules::MutableCursor(ledger, kGmDomain);
	ASSERT_TRUE(rules::RecordDecision(cursor, MakeDecision(10, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 100)));
	const std::string before = ledger.SerializeAsString();

	EXPECT_FALSE(rules::RecordDecision(cursor, MakeDecision(10, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 100)))
		<< "同一 seq 不能记两次";
	EXPECT_FALSE(rules::RecordDecision(cursor, MakeDecision(7, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 70)))
		<< "水位以下不能记";
	EXPECT_FALSE(rules::RecordDecision(cursor, MakeDecision(11, player_inbox::PLAYER_INBOX_OUTCOME_UNKNOWN, 110)))
		<< "UNKNOWN 只进确认、不入环";
	EXPECT_FALSE(rules::RecordDecision(cursor, MakeDecision(12, player_inbox::PLAYER_INBOX_OUTCOME_UNSPECIFIED, 120)));
	EXPECT_EQ(before, ledger.SerializeAsString()) << "前置不满足时不改动任何状态";
}

TEST(PlayerInboxRules, RingKeepsNewestEntriesUpToCapacity)
{
	PlayerInboxLedgerComp ledger;
	RecordApplied(ledger, 1, 40);
	const auto *cursor = rules::FindCursor(ledger, kGmDomain);
	ASSERT_NE(nullptr, cursor);
	EXPECT_EQ(40u, cursor->decided_through_seq());
	ASSERT_EQ(rules::kRecentCapacity, cursor->recent_size());
	EXPECT_EQ(9u, cursor->recent(0).seq()) << "超过 32 条挤掉最旧的";
	EXPECT_EQ(40u, cursor->recent(rules::kRecentCapacity - 1).seq());
	EXPECT_TRUE(rules::ValidateLedger(ledger).empty());
}

TEST(PlayerInboxRules, MutableCursorIsFoundByDomainFieldAndKeepsDomainsUnique)
{
	PlayerInboxLedgerComp ledger;
	rules::MutableCursor(ledger, 5).set_decided_through_seq(55);
	rules::MutableCursor(ledger, 2).set_decided_through_seq(22);
	EXPECT_EQ(55u, rules::MutableCursor(ledger, 5).decided_through_seq()) << "已有的域不重复建";

	ASSERT_EQ(2, ledger.cursors_size());
	EXPECT_EQ(2u, ledger.cursors(0).domain());
	EXPECT_EQ(5u, ledger.cursors(1).domain());
	EXPECT_EQ(nullptr, rules::FindCursor(ledger, 3));
	ASSERT_NE(nullptr, rules::FindCursor(ledger, 2));
	EXPECT_EQ(22u, rules::FindCursor(ledger, 2)->decided_through_seq());
}

// ── 投递分类(IB-9)──────────────────────────────────────────────────────────

TEST(PlayerInboxRules, ClassifyDeliveryAboveWatermarkIsNew)
{
	EXPECT_EQ(rules::DeliveryClass::kNew, rules::ClassifyDelivery(nullptr, 1, 10, kDebtType, nullptr))
		<< "从未处理过的域";

	PlayerInboxLedgerComp ledger;
	RecordApplied(ledger, 1, 3);
	EXPECT_EQ(rules::DeliveryClass::kNew,
			  rules::ClassifyDelivery(rules::FindCursor(ledger, kGmDomain), 4, 999, kBlockType, nullptr));
}

TEST(PlayerInboxRules, ClassifyDeliveryReplaysOnlyWhenSeqRequestIdAndTypeAllMatch)
{
	PlayerInboxLedgerComp ledger;
	auto &cursor = rules::MutableCursor(ledger, kGmDomain);
	ASSERT_TRUE(rules::RecordDecision(
		cursor, MakeDecision(7, player_inbox::PLAYER_INBOX_OUTCOME_REJECTED, 70, kDebtType, 1005)));

	PlayerInboxDecision replayed;
	EXPECT_EQ(rules::DeliveryClass::kReplay, rules::ClassifyDelivery(&cursor, 7, 70, kDebtType, &replayed));
	EXPECT_EQ(Outcome(player_inbox::PLAYER_INBOX_OUTCOME_REJECTED), replayed.outcome()) << "回放原结局";
	EXPECT_EQ(1005u, replayed.reason());

	EXPECT_EQ(rules::DeliveryClass::kMismatch, rules::ClassifyDelivery(&cursor, 7, 71, kDebtType, nullptr))
		<< "request_id 不一致:库被恢复或缺陷,不能把别人的结局回放给它";
	EXPECT_EQ(rules::DeliveryClass::kMismatch, rules::ClassifyDelivery(&cursor, 7, 70, kBlockType, nullptr))
		<< "msg_type 不一致";
}

TEST(PlayerInboxRules, ClassifyDeliveryBelowWatermarkOutsideRingIsUnknown)
{
	PlayerInboxLedgerComp ledger;
	RecordApplied(ledger, 1, 40); // 环里只剩 9..40
	const auto *cursor = rules::FindCursor(ledger, kGmDomain);
	EXPECT_EQ(rules::DeliveryClass::kUnknown, rules::ClassifyDelivery(cursor, 3, 30, kDebtType, nullptr));
	EXPECT_EQ(rules::DeliveryClass::kReplay, rules::ClassifyDelivery(cursor, 9, 90, kDebtType, nullptr));
}

// ── 落盘判定(设计 §7.7)─────────────────────────────────────────────────────

TEST(PlayerInboxRules, RecordedOutcomeIsDurableOnlyWhenSnapshotRingMatches)
{
	const auto applied = MakeDecision(8, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 80);

	PlayerInboxLedgerComp persisted;
	EXPECT_EQ(rules::Durability::kNotYet, rules::ClassifyDurability(persisted, kGmDomain, applied))
		<< "快照里还没有这个域";

	auto &cursor = rules::MutableCursor(persisted, kGmDomain);
	ASSERT_TRUE(rules::RecordDecision(cursor, MakeDecision(5, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 50)));
	EXPECT_EQ(rules::Durability::kNotYet, rules::ClassifyDurability(persisted, kGmDomain, applied))
		<< "快照水位还没追上";

	ASSERT_TRUE(rules::RecordDecision(cursor, applied));
	EXPECT_EQ(rules::Durability::kDurable, rules::ClassifyDurability(persisted, kGmDomain, applied));

	const auto differs = MakeDecision(8, player_inbox::PLAYER_INBOX_OUTCOME_REJECTED, 80, kDebtType, 1005);
	EXPECT_EQ(rules::Durability::kOutcomeDiffers, rules::ClassifyDurability(persisted, kGmDomain, differs))
		<< "结局固定被破坏:不报 durable";
	const auto otherRequest = MakeDecision(8, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 81);
	EXPECT_EQ(rules::Durability::kOutcomeDiffers, rules::ClassifyDurability(persisted, kGmDomain, otherRequest));
}

TEST(PlayerInboxRules, RecordedOutcomeEvictedFromSnapshotRingIsNotDurable)
{
	PlayerInboxLedgerComp persisted;
	RecordApplied(persisted, 1, 40);
	EXPECT_EQ(rules::Durability::kRingMissing,
			  rules::ClassifyDurability(persisted, kGmDomain, MakeDecision(3, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 30)));
}

TEST(PlayerInboxRules, UnknownOutcomeNeedsOnlyTheWatermark)
{
	// UNKNOWN 按定义就不在环里:只要快照水位 ≥ seq 就算落盘。要求它在环里会让这条流永远确认不了(v0.1 的缺陷)。
	const auto unknown = MakeDecision(3, player_inbox::PLAYER_INBOX_OUTCOME_UNKNOWN, 30);

	PlayerInboxLedgerComp persisted;
	EXPECT_EQ(rules::Durability::kNotYet, rules::ClassifyDurability(persisted, kGmDomain, unknown))
		<< "新实体还没有任何落盘:等第一次存盘";

	RecordApplied(persisted, 1, 2);
	EXPECT_EQ(rules::Durability::kNotYet, rules::ClassifyDurability(persisted, kGmDomain, unknown));

	RecordApplied(persisted, 3, 40); // seq 3 早已被挤出环
	EXPECT_EQ(rules::Durability::kDurable, rules::ClassifyDurability(persisted, kGmDomain, unknown));
}

// ── 加载校验(设计 §8 #19)──────────────────────────────────────────────────

TEST(PlayerInboxRules, ValidateLedgerAcceptsEmptyAndWellFormedLedgers)
{
	PlayerInboxLedgerComp ledger;
	EXPECT_TRUE(rules::ValidateLedger(ledger).empty()) << "旧存档没有这个字段 = 空账本,必须合法";

	RecordApplied(ledger, 1, 40);
	rules::MutableCursor(ledger, 2).set_decided_through_seq(7); // 只有水位、环为空也合法
	EXPECT_TRUE(rules::ValidateLedger(ledger).empty()) << rules::ValidateLedger(ledger);
}

TEST(PlayerInboxRules, ValidateLedgerRejectsCorruption)
{
	{
		PlayerInboxLedgerComp ledger;
		ledger.add_cursors()->set_domain(0);
		EXPECT_FALSE(rules::ValidateLedger(ledger).empty()) << "domain 0";
	}
	{
		PlayerInboxLedgerComp ledger;
		ledger.add_cursors()->set_domain(kGmDomain);
		ledger.add_cursors()->set_domain(kGmDomain);
		EXPECT_FALSE(rules::ValidateLedger(ledger).empty()) << "同一个域两条游标";
	}
	{
		PlayerInboxLedgerComp ledger;
		auto *cursor = ledger.add_cursors();
		cursor->set_domain(kGmDomain);
		cursor->set_decided_through_seq(100);
		for (uint64_t seq = 1; seq <= static_cast<uint64_t>(rules::kRecentCapacity) + 1; ++seq)
		{
			*cursor->add_recent() = MakeDecision(seq, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, seq);
		}
		EXPECT_FALSE(rules::ValidateLedger(ledger).empty()) << "环超过 32 条";
	}
	const auto withRecent = [](std::initializer_list<PlayerInboxDecision> recent, uint64_t through)
	{
		PlayerInboxLedgerComp ledger;
		auto *cursor = ledger.add_cursors();
		cursor->set_domain(kGmDomain);
		cursor->set_decided_through_seq(through);
		for (const auto &decision : recent)
		{
			*cursor->add_recent() = decision;
		}
		return ledger;
	};
	EXPECT_FALSE(rules::ValidateLedger(withRecent({MakeDecision(0, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 1)}, 5))
					 .empty())
		<< "seq 0";
	EXPECT_FALSE(rules::ValidateLedger(withRecent({MakeDecision(4, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 1),
												   MakeDecision(4, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 2)},
												  5))
					 .empty())
		<< "不严格升序";
	EXPECT_FALSE(rules::ValidateLedger(withRecent({MakeDecision(6, player_inbox::PLAYER_INBOX_OUTCOME_APPLIED, 1)}, 5))
					 .empty())
		<< "环里的 seq 超过水位";
	EXPECT_FALSE(rules::ValidateLedger(withRecent({MakeDecision(3, player_inbox::PLAYER_INBOX_OUTCOME_UNKNOWN, 1)}, 5))
					 .empty())
		<< "UNKNOWN 不该出现在环里";
}

// ── 触发与时长 ──────────────────────────────────────────────────────────────

TEST(PlayerInboxRules, NewLandingMeansFreshEntityOrChangedOwnerEpoch)
{
	EXPECT_TRUE(rules::IsNewLanding(/*hasRuntime=*/false, 0, 5)) << "实体在本节点新建";
	EXPECT_TRUE(rules::IsNewLanding(/*hasRuntime=*/false, 0, 0)) << "新建实体即便 epoch 未铸造也要排一次(由前置闸延后)";
	EXPECT_TRUE(rules::IsNewLanding(/*hasRuntime=*/true, 5, 6)) << "owner_epoch 变了 = 归属变过";
	EXPECT_FALSE(rules::IsNewLanding(/*hasRuntime=*/true, 5, 5)) << "同节点换图 / gate 补发的重复路由";
}

TEST(PlayerInboxRules, BackoffDoublesFromOneSecondAndCapsAtOneMinute)
{
	constexpr uint32_t kMidJitter = 500; // 抖动因子 = 1.0
	EXPECT_EQ(1000u, rules::BackoffMs(0, kMidJitter)) << "0 按 1 处理";
	EXPECT_EQ(1000u, rules::BackoffMs(1, kMidJitter));
	EXPECT_EQ(2000u, rules::BackoffMs(2, kMidJitter));
	EXPECT_EQ(4000u, rules::BackoffMs(3, kMidJitter));
	EXPECT_EQ(32000u, rules::BackoffMs(6, kMidJitter));
	EXPECT_EQ(60000u, rules::BackoffMs(7, kMidJitter));
	EXPECT_EQ(60000u, rules::BackoffMs(1000, kMidJitter));

	EXPECT_EQ(1000u, rules::BackoffMs(1, 0)) << "下抖后仍夹在 1 秒";
	EXPECT_EQ(1200u, rules::BackoffMs(1, 1000));
	EXPECT_EQ(48000u, rules::BackoffMs(100, 0));
	EXPECT_EQ(60000u, rules::BackoffMs(100, 1000)) << "上抖后仍夹在 60 秒";
	EXPECT_EQ(rules::BackoffMs(2, 1000), rules::BackoffMs(2, 5000)) << "抖动参数越界按 1000 处理";
}

TEST(PlayerInboxRules, PeriodicDelayIsMinutesWithTwentyPercentJitterAndZeroDisables)
{
	EXPECT_EQ(0u, rules::PeriodicDelayMs(0, 500)) << "0 = 关闭在线兜底";
	EXPECT_EQ(1800000u, rules::PeriodicDelayMs(30, 500));
	EXPECT_EQ(1440000u, rules::PeriodicDelayMs(30, 0));
	EXPECT_EQ(2160000u, rules::PeriodicDelayMs(30, 1000));
}
