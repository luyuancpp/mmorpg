#include "bag_service.h"

#include "engine/core/error_handling/error_handling.h"
#include "engine/core/macros/return_define.h"
#include "table/proto/tip/common_error_tip.pb.h"

#include "modules/gain_block/gain_block_service.h"
#include "modules/transaction_log/anomaly_detector.h"
#include "modules/transaction_log/transaction_log_system.h"
#include "services/scene/player/system/player_lifecycle.h" // IsCrossZoneFrozen — cross-zone-readiness-audit.md §11.1

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

	// ── Cross-zone Frozen check (Single Writer guarantee) ────────────────
	// Bag is part of the player's marshaled state; once a transfer is in
	// flight any item add on the source side never reaches the destination
	// and will be lost when DestroyPlayer fires on ACK. Reject early — also
	// avoids polluting transaction_log with TX_SYSTEM_GRANT entries for items
	// that effectively don't exist.
	// See docs/design/cross-zone-readiness-audit.md §11.1.
	if (PlayerLifecycleSystem::IsCrossZoneFrozen(playerEntity))
	{
		LOG_WARN << "BagService::AddItem rejected: player frozen for cross-zone migration. "
				 << "config_id=" << configId << " player=" << bag.PlayerGuid();
		return PrintStackAndReturnError(kInvalidParameter);
	}

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
			playerEntity, Bag::LastGeneratedItemGuid(),
			param.itemPBComp.config_id(),
			param.itemPBComp.size(),
			TX_SYSTEM_GRANT);

		AnomalyDetector::RecordItemGain(
			playerEntity, param.itemPBComp.config_id(),
			param.itemPBComp.size());
	}

	return result;
}

uint32_t BagService::AddItems(
	entt::entity playerEntity,
	Bag &bag,
	const PlayerItemBlockList &blockList,
	const ItemCountMap &itemsToAdd)
{
	// ── Cross-zone Frozen check (Single Writer guarantee) ────────────────
	// See AddItem above for rationale. Reject the whole batch early.
	// See docs/design/cross-zone-readiness-audit.md §11.1.
	if (PlayerLifecycleSystem::IsCrossZoneFrozen(playerEntity))
	{
		LOG_WARN << "BagService::AddItems rejected: player frozen for cross-zone migration. "
				 << "player=" << bag.PlayerGuid();
		return PrintStackAndReturnError(kInvalidParameter);
	}

	// ── Block checks first (transactional: reject whole batch if any blocked) ──
	for (const auto &[configId, count] : itemsToAdd)
	{
		if (GainBlockService::IsGainBlocked(GainBlockService::GainType::kItem, configId))
		{
			LOG_WARN << "BagService::AddItems: item GLOBALLY blocked. config_id="
					 << configId << " player=" << bag.PlayerGuid();
			return PrintStackAndReturnError(kInvalidParameter);
		}

		if (blockList.IsBlocked(configId))
		{
			LOG_WARN << "BagService::AddItems: item blocked by GM. config_id="
					 << configId << " player=" << bag.PlayerGuid();
			return PrintStackAndReturnError(kInvalidParameter);
		}
	}

	// ── All-or-nothing space check before any mutation ───────────────────
	RETURN_ON_ERROR(bag.HasEnoughSpace(itemsToAdd));

	// ── Apply per config: Bag::AddItem → transaction log + anomaly ───────
	for (const auto &[configId, count] : itemsToAdd)
	{
		InitItemParam param;
		param.itemPBComp.set_config_id(configId);
		param.itemPBComp.set_size(count);

		auto result = bag.AddItem(param);
		if (result != kSuccess)
		{
			return result;
		}

		TransactionLogSystem::LogItemCreate(
			playerEntity, Bag::LastGeneratedItemGuid(),
			configId, count, TX_SYSTEM_GRANT);

		AnomalyDetector::RecordItemGain(playerEntity, configId, count);
	}

	return kSuccess;
}

uint32_t BagService::AddItems(
	entt::entity playerEntity,
	Bag &bag,
	const PlayerItemBlockList &blockList,
	const std::vector<InitItemParam> &itemsToAdd)
{
	// ── Cross-zone Frozen check (Single Writer guarantee) ────────────────
	// See AddItem above for rationale. Reject the whole batch early.
	// See docs/design/cross-zone-readiness-audit.md §11.1.
	if (PlayerLifecycleSystem::IsCrossZoneFrozen(playerEntity))
	{
		LOG_WARN << "BagService::AddItems rejected: player frozen for cross-zone migration. "
				 << "player=" << bag.PlayerGuid();
		return PrintStackAndReturnError(kInvalidParameter);
	}

	// ── Block checks first (transactional: reject whole batch if any blocked) ──
	for (const auto &param : itemsToAdd)
	{
		const auto configId = param.itemPBComp.config_id();
		if (GainBlockService::IsGainBlocked(GainBlockService::GainType::kItem, configId))
		{
			LOG_WARN << "BagService::AddItems: item GLOBALLY blocked. config_id="
					 << configId << " player=" << bag.PlayerGuid();
			return PrintStackAndReturnError(kInvalidParameter);
		}

		if (blockList.IsBlocked(configId))
		{
			LOG_WARN << "BagService::AddItems: item blocked by GM. config_id="
					 << configId << " player=" << bag.PlayerGuid();
			return PrintStackAndReturnError(kInvalidParameter);
		}
	}

	// ── All-or-nothing space pre-check before any mutation ───────────────
	// Aggregate config -> total size; HasEnoughSpace handles equipment
	// (maxStack==1, one grid per unit) and stackables uniformly.
	ItemCountMap requiredSpace;
	for (const auto &param : itemsToAdd)
	{
		requiredSpace[param.itemPBComp.config_id()] += param.itemPBComp.size();
	}
	RETURN_ON_ERROR(bag.HasEnoughSpace(requiredSpace));

	// ── Apply per piece, interleaving the transaction log ────────────────
	// Add one piece at a time so LastGeneratedItemGuid() yields the guid
	// actually minted for that piece. Equipment with a preassigned guid keeps
	// it; stackables / multi-piece adds use the freshly minted guid. One
	// transaction_log + anomaly record per piece, matching single AddItem.
	for (const auto &param : itemsToAdd)
	{
		auto result = bag.AddItem(param);
		if (result != kSuccess)
		{
			return result;
		}

		const auto preassignedGuid = param.itemPBComp.item_id();
		const auto loggedGuid = (preassignedGuid != kInvalidGuid && preassignedGuid > 0)
									 ? preassignedGuid
									 : Bag::LastGeneratedItemGuid();

		TransactionLogSystem::LogItemCreate(
			playerEntity, loggedGuid,
			param.itemPBComp.config_id(),
			param.itemPBComp.size(),
			TX_SYSTEM_GRANT);

		AnomalyDetector::RecordItemGain(
			playerEntity, param.itemPBComp.config_id(),
			param.itemPBComp.size());
	}

	return kSuccess;
}

uint32_t BagService::RemoveItem(
	entt::entity playerEntity,
	Bag &bag,
	Guid guid)
{
	// ── Cross-zone Frozen check (Single Writer guarantee) ────────────────
	// See AddItem above for rationale. Removing an item from the source-
	// side bag while a transfer is in flight would leave the destination
	// with a duplicate (the marshaled PlayerAllData still contains the item)
	// — worse than rejecting the remove.
	// See docs/design/cross-zone-readiness-audit.md §11.1.
	if (PlayerLifecycleSystem::IsCrossZoneFrozen(playerEntity))
	{
		LOG_WARN << "BagService::RemoveItem rejected: player frozen for cross-zone migration. "
				 << "guid=" << guid << " player=" << bag.PlayerGuid();
		return PrintStackAndReturnError(kInvalidParameter);
	}

	// Capture item info for transaction log before destroying.
	auto *itemComp = bag.GetItemCompByGuid(guid);
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
