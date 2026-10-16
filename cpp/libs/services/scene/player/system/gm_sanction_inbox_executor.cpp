#include "player/system/gm_sanction_inbox_executor.h"

#include <cstdint>
#include <limits>

#include "muduo/base/Logging.h"

#include "engine/core/type_define/player_guid_comp.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/currency/constants/currency.h"
#include "modules/currency/system/currency_system.h"
#include "proto/common/component/currency_comp.pb.h"
#include "proto/player_inbox/player_inbox.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/player_inbox_error_tip.pb.h"
#include "thread_context/ecs_context.h"

namespace
{
	PlayerInboxExecResult Applied()
	{
		return PlayerInboxExecResult{PlayerInboxExecKind::kApplied, 0};
	}

	PlayerInboxExecResult Rejected(uint32_t tipId)
	{
		return PlayerInboxExecResult{PlayerInboxExecKind::kRejected, tipId};
	}

	// 原语的 tip 码 → 结局。kSuccess 之外的码都是确定性拒绝(原语只在参数 / 状态不可能成功时拒绝)。
	PlayerInboxExecResult FromTip(uint32_t tipId)
	{
		return tipId == static_cast<uint32_t>(kSuccess) ? Applied() : Rejected(tipId);
	}

	uint64_t GuidForLog(entt::entity player)
	{
		const auto *guid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(player);
		return guid != nullptr ? guid->value : 0;
	}

	// 币种必须在 CurrencyType 范围内。纯判定,与 CurrencySystem 的同名校验同口径;放在执行器里先判,
	// 是为了把"载荷写错了"统一报成 kPlayerInboxPayloadInvalid,而不是原语的通用 kInvalidParameter。
	bool IsKnownCurrencyType(uint32_t currencyType)
	{
		return currencyType < static_cast<uint32_t>(kCurrencyMax);
	}

	// CurrencyComp 与 PlayerCurrencyComp 都是只由加载创建的持久组件。缺了说明加载不变量已被破坏:
	// 不改任何状态、回 kDefer,不把这条 GM 处置因本地缺陷钉成终态 REJECTED(ecs-component-access-rules.md 规则 4)。
	bool HasCurrencyState(entt::entity player, const char *executor)
	{
		if (tlsEcs.actorRegistry.all_of<CurrencyComp, PlayerCurrencyComp>(player))
		{
			return true;
		}
		LOG_ERROR << "[PlayerInbox] " << executor
				  << " deferred: persisted currency component missing (load invariant broken) player_id="
				  << GuidForLog(player)
				  << " has_currency=" << tlsEcs.actorRegistry.all_of<CurrencyComp>(player)
				  << " has_player_currency=" << tlsEcs.actorRegistry.all_of<PlayerCurrencyComp>(player);
		return false;
	}

	PlayerInboxExecResult Deferred()
	{
		return PlayerInboxExecResult{PlayerInboxExecKind::kDefer, 0};
	}

	// 两种封锁类型共用同一个载荷,解码与币种校验也相同;成功时把币种交回调用方。
	bool DecodeGainPayload(const std::string &payload, uint32_t &currencyType)
	{
		player_inbox::GmCurrencyGainPayload decoded;
		if (!decoded.ParseFromString(payload) || !IsKnownCurrencyType(decoded.currency_type()))
		{
			return false;
		}
		currencyType = decoded.currency_type();
		return true;
	}
} // namespace

PlayerInboxExecResult GmSanctionInboxExecutor::BlockCurrencyGain(entt::entity player, const std::string &payload)
{
	uint32_t currencyType = 0;
	if (!DecodeGainPayload(payload, currencyType))
	{
		return Rejected(kPlayerInboxPayloadInvalid);
	}
	if (!HasCurrencyState(player, "BlockCurrencyGain"))
	{
		return Deferred();
	}
	return FromTip(CurrencySystem::BlockCurrency(player, static_cast<CurrencyType>(currencyType)));
}

PlayerInboxExecResult GmSanctionInboxExecutor::UnblockCurrencyGain(entt::entity player, const std::string &payload)
{
	uint32_t currencyType = 0;
	if (!DecodeGainPayload(payload, currencyType))
	{
		return Rejected(kPlayerInboxPayloadInvalid);
	}
	if (!HasCurrencyState(player, "UnblockCurrencyGain"))
	{
		return Deferred();
	}
	return FromTip(CurrencySystem::UnblockCurrency(player, static_cast<CurrencyType>(currencyType)));
}

PlayerInboxExecResult GmSanctionInboxExecutor::AttachCurrencyDebt(entt::entity player, const std::string &payload)
{
	player_inbox::GmCurrencyDebtPayload decoded;
	if (!decoded.ParseFromString(payload) || !IsKnownCurrencyType(decoded.currency_type()))
	{
		return Rejected(kPlayerInboxPayloadInvalid);
	}
	// 货币在协议里是 uint64(AGENTS §4),AttachDebt 的参数是 int64_t:入箱时已校验 ∈ (0, INT64_MAX],
	// 这里再判一次,越界的载荷不能靠窄化变成负数或 0。
	constexpr uint64_t kMaxOweAmount = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
	if (decoded.owe_amount() == 0 || decoded.owe_amount() > kMaxOweAmount)
	{
		return Rejected(kPlayerInboxPayloadInvalid);
	}
	if (!HasCurrencyState(player, "AttachCurrencyDebt"))
	{
		return Deferred();
	}
	return FromTip(CurrencySystem::AttachDebt(player, static_cast<CurrencyType>(decoded.currency_type()),
											  static_cast<int64_t>(decoded.owe_amount()), decoded.reason(),
											  decoded.gm_operator(), decoded.expires_at()));
}
