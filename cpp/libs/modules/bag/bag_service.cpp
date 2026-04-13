#include "bag_service.h"

#include "engine/core/error_handling/error_handling.h"
#include "engine/core/macros/return_define.h"
#include "table/proto/tip/common_error_tip.pb.h"

#include "modules/gain_block/gain_block_service.h"
#include "modules/transaction_log/anomaly_detector.h"
#include "modules/transaction_log/transaction_log_system.h"

// ---------------------------------------------------------------------------
// PlayerItemBlockList
// ---------------------------------------------------------------------------

void PlayerItemBlockList::Block(uint32_t configId)
{
	blocked_config_ids_.insert(configId);
}

void PlayerItemBlockList::Unblock(uint32_t configId)
{
	blocked_config_ids_.erase(configId);
}

bool PlayerItemBlockList::IsBlocked(uint32_t configId) const
{
	return blocked_config_ids_.contains(configId);
}

// ---------------------------------------------------------------------------
// BagService
// ---------------------------------------------------------------------------

uint32_t BagService::AddItem(
	entt::entity playerEntity,
	Bag &bag,
	const PlayerItemBlockList &blockList,
	const InitItemParam &param)
{
	auto configId = param.itemPBComp.config_id();

	// ── Global (server-wide) block check ─────────────────────────────────
	if (GainBlockService::IsGainBlocked(GainBlockService::GainType::kItem, configId))
	{
		LOG_WARN << "BagService::AddItem: item GLOBALLY blocked. config_id="
				 << configId << " player=" << bag.PlayerGuid();
		return PrintStackAndReturnError(kInvalidParameter);
	}

	// ── Per-player GM block check ────────────────────────────────────────
	if (blockList.IsBlocked(configId))
	{
		LOG_WARN << "BagService::AddItem: item blocked by GM. config_id="
				 << configId << " player=" << bag.PlayerGuid();
		return PrintStackAndReturnError(kInvalidParameter);
	}

	// ── Delegate to pure container ───────────────────────────────────────
	auto result = bag.AddItem(param);

	// ── Post-success: transaction log + anomaly detection ────────────────
	if (result == kSuccess)
	{
		TransactionLogSystem::LogItemCreate(
			playerEntity, Bag::LastGeneratorItemGuid(),
			param.itemPBComp.config_id(),
			param.itemPBComp.size(),
			TX_SYSTEM_GRANT);

		AnomalyDetector::RecordItemGain(
			playerEntity, param.itemPBComp.config_id(),
			param.itemPBComp.size());
	}

	return result;
}

uint32_t BagService::RemoveItem(
	entt::entity playerEntity,
	Bag &bag,
	Guid guid)
{
	// Capture item info for transaction log before destroying.
	auto *itemComp = bag.GetItemBaseByGuid(guid);
	uint32_t capturedConfigId = 0;
	uint32_t capturedSize = 0;
	if (itemComp != nullptr)
	{
		capturedConfigId = itemComp->config_id();
		capturedSize = itemComp->size();
	}

	auto result = bag.RemoveItem(guid);

	if (result == kSuccess && capturedConfigId > 0)
	{
		TransactionLogSystem::LogItemDestroy(
			playerEntity, guid,
			capturedConfigId, capturedSize);
	}

	return result;
}
