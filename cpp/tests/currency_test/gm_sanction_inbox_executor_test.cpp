// 收件箱 SCENE_GM_SANCTION 域执行器与类型注册表的用例(docs/design/player-inbox-service.md §2.2、§3.2、§7.6)。
//
// 执行器只依赖 CurrencySystem 与两份货币组件:这里手搭一个"加载已完成"的实体,喂序列化好的载荷,
// 验结局(APPLIED / REJECTED / DEFER)与货币状态。注册表用例扫描 proto 描述符,保证每个类型都有执行器、
// 规格与执行器表不脱节。不连 Redis、不发 gRPC、不读真实墙钟。

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/unknown_field_set.h>

#include "engine/core/type_define/player_guid_comp.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/currency/constants/currency.h"
#include "modules/currency/system/currency_system.h"
#include "player/system/gm_sanction_inbox_executor.h"
#include "player/system/player_inbox_registry.h"
#include "proto/common/component/currency_comp.pb.h"
#include "proto/player_inbox/player_inbox.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/player_inbox_error_tip.pb.h"
#include "thread_context/ecs_context.h"

namespace
{
	constexpr uint64_t kTestPlayerId = 950200001;
	constexpr uint32_t kGmDomain = player_inbox::PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION;
	constexpr uint32_t kGold = static_cast<uint32_t>(kCurrencyGold);

	std::string GainPayload(uint32_t currencyType)
	{
		player_inbox::GmCurrencyGainPayload payload;
		payload.set_currency_type(currencyType);
		return payload.SerializeAsString();
	}

	std::string DebtPayload(uint32_t currencyType, uint64_t oweAmount, const std::string &reason = "dup-grant",
							const std::string &gmOperator = "ops-1", uint64_t expiresAt = 0)
	{
		player_inbox::GmCurrencyDebtPayload payload;
		payload.set_currency_type(currencyType);
		payload.set_owe_amount(oweAmount);
		payload.set_reason(reason);
		payload.set_gm_operator(gmOperator);
		payload.set_expires_at(expiresAt);
		return payload.SerializeAsString();
	}

	// 字段 1 的 varint 标签后面没有值:任何消息都解不出来。
	const std::string kTruncatedPayload("\x08", 1);

	class GmSanctionInboxExecutorTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			player_ = tlsEcs.actorRegistry.create();
			tlsEcs.actorRegistry.emplace<PlayerGuidComp>(player_, kTestPlayerId);
			// 两份货币组件都只由加载创建(PlayerDatabaseMessageFieldsUnmarshal);这里在测试创建点显式挂上。
			tlsEcs.actorRegistry.emplace<CurrencyComp>(player_);
			tlsEcs.actorRegistry.emplace<PlayerCurrencyComp>(player_);
		}

		void TearDown() override
		{
			if (tlsEcs.actorRegistry.valid(player_))
			{
				tlsEcs.actorRegistry.destroy(player_);
			}
		}

		const CurrencyDebt *GoldDebt() const
		{
			const auto &debts = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player_).debts;
			const auto it = debts.find(kGold);
			return it == debts.end() ? nullptr : &it->second;
		}

		entt::entity player_{entt::null};
	};
} // namespace

// ── 封锁 / 解封 ─────────────────────────────────────────────────────────────

TEST_F(GmSanctionInboxExecutorTest, BlockCurrencyGainAppliesAndIsIdempotent)
{
	const auto first = GmSanctionInboxExecutor::BlockCurrencyGain(player_, GainPayload(kGold));
	EXPECT_EQ(PlayerInboxExecKind::kApplied, first.kind);
	EXPECT_EQ(0u, first.reason);
	EXPECT_TRUE(CurrencySystem::IsCurrencyBlocked(player_, kCurrencyGold));

	const auto again = GmSanctionInboxExecutor::BlockCurrencyGain(player_, GainPayload(kGold));
	EXPECT_EQ(PlayerInboxExecKind::kApplied, again.kind) << "已封锁时直接成功(天然幂等)";
	EXPECT_EQ(1, tlsEcs.actorRegistry.get<CurrencyComp>(player_).blocked_types_size()) << "不重复登记";
}

TEST_F(GmSanctionInboxExecutorTest, UnblockCurrencyGainAppliesWhetherOrNotBlocked)
{
	EXPECT_EQ(PlayerInboxExecKind::kApplied,
			  GmSanctionInboxExecutor::UnblockCurrencyGain(player_, GainPayload(kGold)).kind)
		<< "本来就没封锁:天然幂等,照样 APPLIED";

	ASSERT_EQ(PlayerInboxExecKind::kApplied, GmSanctionInboxExecutor::BlockCurrencyGain(player_, GainPayload(kGold)).kind);
	EXPECT_EQ(PlayerInboxExecKind::kApplied,
			  GmSanctionInboxExecutor::UnblockCurrencyGain(player_, GainPayload(kGold)).kind);
	EXPECT_FALSE(CurrencySystem::IsCurrencyBlocked(player_, kCurrencyGold));
}

TEST_F(GmSanctionInboxExecutorTest, GainPayloadWithUnknownCurrencyOrGarbageIsRejected)
{
	const auto unknownType =
		GmSanctionInboxExecutor::BlockCurrencyGain(player_, GainPayload(static_cast<uint32_t>(kCurrencyMax)));
	EXPECT_EQ(PlayerInboxExecKind::kRejected, unknownType.kind);
	EXPECT_EQ(static_cast<uint32_t>(kPlayerInboxPayloadInvalid), unknownType.reason);

	const auto garbage = GmSanctionInboxExecutor::UnblockCurrencyGain(player_, kTruncatedPayload);
	EXPECT_EQ(PlayerInboxExecKind::kRejected, garbage.kind) << "认识的类型、载荷解不出来:确定性拒绝,记账";
	EXPECT_EQ(static_cast<uint32_t>(kPlayerInboxPayloadInvalid), garbage.reason);

	EXPECT_EQ(0, tlsEcs.actorRegistry.get<CurrencyComp>(player_).blocked_types_size()) << "拒绝路径不改状态";
}

// ── 挂欠款 ──────────────────────────────────────────────────────────────────

TEST_F(GmSanctionInboxExecutorTest, AttachCurrencyDebtAppliesWithPayloadMetadata)
{
	const auto result =
		GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 100, "dup-grant", "ops-1", 1893456000));
	EXPECT_EQ(PlayerInboxExecKind::kApplied, result.kind);

	const auto *debt = GoldDebt();
	ASSERT_NE(nullptr, debt);
	EXPECT_EQ(100u, debt->owed);
	EXPECT_EQ("dup-grant", debt->reason);
	EXPECT_EQ("ops-1", debt->gmOperator);
	EXPECT_EQ(1893456000u, debt->expiresAt);
}

TEST_F(GmSanctionInboxExecutorTest, AttachCurrencyDebtIsNotIdempotentByItself)
{
	// 这正是收件箱必须靠账本去重的原因(设计 §2.2):同一条载荷执行两次就挂两次。
	ASSERT_EQ(PlayerInboxExecKind::kApplied,
			  GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 100)).kind);
	ASSERT_EQ(PlayerInboxExecKind::kApplied,
			  GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 100)).kind);
	ASSERT_NE(nullptr, GoldDebt());
	EXPECT_EQ(200u, GoldDebt()->owed);
}

TEST_F(GmSanctionInboxExecutorTest, AttachCurrencyDebtRejectsAmountOutsideInt64Range)
{
	const auto zero = GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 0));
	EXPECT_EQ(PlayerInboxExecKind::kRejected, zero.kind);
	EXPECT_EQ(static_cast<uint32_t>(kPlayerInboxPayloadInvalid), zero.reason);

	const uint64_t beyondInt64 = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
	const auto tooLarge = GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, beyondInt64));
	EXPECT_EQ(PlayerInboxExecKind::kRejected, tooLarge.kind) << "不能靠窄化变成负数";
	EXPECT_EQ(static_cast<uint32_t>(kPlayerInboxPayloadInvalid), tooLarge.reason);

	const auto unknownType =
		GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(static_cast<uint32_t>(kCurrencyMax), 10));
	EXPECT_EQ(PlayerInboxExecKind::kRejected, unknownType.kind);

	const auto garbage = GmSanctionInboxExecutor::AttachCurrencyDebt(player_, kTruncatedPayload);
	EXPECT_EQ(PlayerInboxExecKind::kRejected, garbage.kind);

	EXPECT_TRUE(tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player_).debts.empty()) << "拒绝路径不落欠款";
}

TEST_F(GmSanctionInboxExecutorTest, AttachCurrencyDebtOverflowIsRejectedWithPrimitiveCode)
{
	auto &debt = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player_).debts[kGold];
	debt.owed = std::numeric_limits<uint64_t>::max() - 5;

	const auto result = GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 10));
	EXPECT_EQ(PlayerInboxExecKind::kRejected, result.kind) << "AttachDebt 回的非成功码原样成为拒绝原因";
	EXPECT_EQ(static_cast<uint32_t>(kInvalidParameter), result.reason);
	EXPECT_EQ(std::numeric_limits<uint64_t>::max() - 5, debt.owed);
}

// ── 版本错位(混版本保护)────────────────────────────────────────────────────

// 同币种的既有欠款带着本版本不认识的字段(新版本写下的):AttachDebt 不会合并,执行器报版本错位,
// 不能记成终态 REJECTED —— 那样这笔 GM 欠款以后再也不会挂上(docs/design/mixed-version-persist-field-protection.md §4.4,
// player-inbox-service.md §7.6)。也不是 DEFER:错位会在会话内消失,本地续做会越过服务端之后挂的 VOID(评审 L1);
// 收件箱据 kVersionSkew 停用本域、下次落地重拉(见 PlayerInboxSystemTest 的同名场景)。别的币种不受影响。
TEST_F(GmSanctionInboxExecutorTest, AttachCurrencyDebtReportsVersionSkewWhenExistingDebtCarriesUnknownFields)
{
	constexpr int kFutureField = 9999; // 本版本不认识的字段号
	CurrencyComp persisted;
	auto *entry = persisted.add_debts();
	entry->set_currency_type(kGold);
	entry->set_owed(500);
	entry->mutable_unknown_fields()->AddVarint(kFutureField, 7);
	auto &runtime = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player_);
	runtime.LoadFromProto(persisted);
	ASSERT_TRUE(CurrencySystem::DebtCarriesUnknownFields(player_, kCurrencyGold));

	const auto skewed = GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 100));
	EXPECT_EQ(PlayerInboxExecKind::kVersionSkew, skewed.kind) << "版本错位绝不能当成终态的 REJECTED,也不走 DEFER 的本地续做";
	ASSERT_NE(nullptr, GoldDebt());
	EXPECT_EQ(500u, GoldDebt()->owed) << "版本错位路径不改欠款";

	CurrencyComp saved;
	ASSERT_TRUE(runtime.SaveToProto(saved));
	ASSERT_EQ(1, saved.debts_size());
	ASSERT_EQ(1, saved.debts(0).unknown_fields().field_count()) << "未知字段原样保留";
	EXPECT_EQ(kFutureField, saved.debts(0).unknown_fields().field(0).number());
	EXPECT_EQ(7u, saved.debts(0).unknown_fields().field(0).varint());

	const uint32_t diamond = static_cast<uint32_t>(kCurrencyDiamond);
	EXPECT_FALSE(CurrencySystem::DebtCarriesUnknownFields(player_, kCurrencyDiamond));
	EXPECT_EQ(PlayerInboxExecKind::kApplied,
			  GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(diamond, 100)).kind)
		<< "另一币种的欠款没有携带未知字段,照常挂";
	EXPECT_EQ(100u, runtime.debts.at(diamond).owed);
}

// ── 加载不变量被破坏 ────────────────────────────────────────────────────────

TEST_F(GmSanctionInboxExecutorTest, MissingCurrencyComponentsDeferWithoutSideEffects)
{
	tlsEcs.actorRegistry.remove<PlayerCurrencyComp>(player_);

	EXPECT_EQ(PlayerInboxExecKind::kDefer, GmSanctionInboxExecutor::BlockCurrencyGain(player_, GainPayload(kGold)).kind)
		<< "不能把一条 GM 处置因本地缺陷钉成终态 REJECTED";
	EXPECT_EQ(PlayerInboxExecKind::kDefer,
			  GmSanctionInboxExecutor::UnblockCurrencyGain(player_, GainPayload(kGold)).kind);
	EXPECT_EQ(PlayerInboxExecKind::kDefer,
			  GmSanctionInboxExecutor::AttachCurrencyDebt(player_, DebtPayload(kGold, 100)).kind);

	EXPECT_EQ(0, tlsEcs.actorRegistry.get<CurrencyComp>(player_).blocked_types_size());
	EXPECT_FALSE(CurrencySystem::DebtCarriesUnknownFields(player_, kCurrencyGold)) << "缺组件时只读谓词返回 false";
	EXPECT_FALSE(tlsEcs.actorRegistry.all_of<PlayerCurrencyComp>(player_)) << "执行器只取不建";

	// 载荷本身坏掉时,确定性拒绝优先于"缺组件"(纯校验先跑)。
	EXPECT_EQ(PlayerInboxExecKind::kRejected, GmSanctionInboxExecutor::AttachCurrencyDebt(player_, kTruncatedPayload).kind);
}

// ── 类型注册表(设计 §3.2)──────────────────────────────────────────────────

TEST(PlayerInboxRegistry, SelfCheckPasses)
{
	EXPECT_EQ(std::string{}, player_inbox_registry::SelfCheck());
}

// 扫描描述符:每个非 0 类型都写了规格,并且在它所属的域上有执行器;换一个域就没有。
TEST(PlayerInboxRegistry, EveryMessageTypeHasExecutorInItsOwnDomainOnly)
{
	const auto *descriptor = player_inbox::PlayerInboxMessageType_descriptor();
	ASSERT_NE(nullptr, descriptor);
	int checked = 0;
	for (int i = 0; i < descriptor->value_count(); ++i)
	{
		const auto *value = descriptor->value(i);
		if (value->number() == 0)
		{
			continue;
		}
		const std::string name(value->name());
		const auto &options = value->options();
		ASSERT_TRUE(options.HasExtension(player_inbox::OptionPlayerInboxType)) << name << " 缺 OptionPlayerInboxType";
		const auto &spec = options.GetExtension(player_inbox::OptionPlayerInboxType);
		const auto msgType = static_cast<uint32_t>(value->number());
		const auto domain = static_cast<uint32_t>(spec.domain());
		EXPECT_NE(nullptr, player_inbox_registry::FindExecutor(msgType, domain))
			<< name << " 在 player_inbox_registry.cpp 的执行器表里没有执行器";
		EXPECT_EQ(nullptr, player_inbox_registry::FindExecutor(msgType, domain + 1000))
			<< name << " 不属于别的域";
		EXPECT_EQ(name, player_inbox_registry::TypeName(msgType));
		++checked;
	}
	EXPECT_EQ(3, checked) << "v1 只有三种 GM 处置类型;加了类型请同步补执行器与本断言";
}

TEST(PlayerInboxRegistry, UnknownTypeOrDomainHasNoExecutor)
{
	const auto block = static_cast<uint32_t>(player_inbox::PLAYER_INBOX_MSG_GM_BLOCK_CURRENCY_GAIN);
	EXPECT_EQ(nullptr, player_inbox_registry::FindExecutor(0, kGmDomain)) << "UNSPECIFIED";
	EXPECT_EQ(nullptr, player_inbox_registry::FindExecutor(999, kGmDomain)) << "本二进制不认识的新类型:不能当 REJECTED";
	EXPECT_EQ(nullptr, player_inbox_registry::FindExecutor(block, 0));
	EXPECT_EQ(nullptr, player_inbox_registry::FindExecutor(block, 999));
	EXPECT_EQ("unknown", player_inbox_registry::TypeName(999));
}

TEST(PlayerInboxRegistry, GmSanctionDomainSpecIsReadFromDescriptor)
{
	const auto rule = player_inbox_registry::DomainRule(kGmDomain);
	EXPECT_TRUE(rule.known);
	EXPECT_TRUE(rule.requiresOutOfBattle) << "GM 处置要求不在战斗中(设计 §7.5)";
	EXPECT_EQ(64u, rule.maxPending);

	const auto unknown = player_inbox_registry::DomainRule(999);
	EXPECT_FALSE(unknown.known);
	EXPECT_TRUE(unknown.requiresOutOfBattle) << "未知域按要求非战斗处理(fail-closed)";
	EXPECT_FALSE(player_inbox_registry::DomainRule(0).known);
}

TEST(PlayerInboxRegistry, ConsumerDomainsAreTheDomainsWithExecutors)
{
	const std::vector<uint32_t> expected{kGmDomain};
	EXPECT_EQ(expected, player_inbox_registry::ConsumerDomains());
}
