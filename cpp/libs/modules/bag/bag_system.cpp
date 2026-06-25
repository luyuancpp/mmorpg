#include "bag_system.h"

#include <vector>

#include "engine/core/error_handling/error_handling.h"
#include "engine/core/macros/return_define.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/bag_error_tip.pb.h"
#include "core/utils/id/snow_flake.h"
#include "table/code/item_table.h"
#include "core/utils/defer/defer.h"
#include <thread_context/snow_flake_manager.h>

std::size_t Bag::GetItemStackSize(uint32_t config_id) const
{
	std::size_t totalSize = 0;
	for (const auto &[entity, item] : itemRegistry_.view<ItemComp>().each())
	{
		if (item.config_id() == config_id)
		{
			totalSize += item.size();
		}
	}
	return totalSize;
}

ItemComp *Bag::GetItemCompByGuid(Guid guid)
{
	auto guidIt = items_.find(guid);
	return (guidIt != items_.end()) ? itemRegistry_.try_get<ItemComp>(guidIt->second) : nullptr;
}

ItemComp *Bag::GetItemCompByPos(uint32_t pos)
{
	auto posIt = pos_.find(pos);
	return (posIt != pos_.end()) ? GetItemCompByGuid(posIt->second) : nullptr;
}

entt::entity Bag::GetItemByGuid(Guid guid)
{
	auto guidIt = items_.find(guid);
	return (guidIt != items_.end()) ? guidIt->second : entt::null;
}

entt::entity Bag::GetItemByPos(uint32_t pos)
{
	auto posIt = pos_.find(pos);
	return (posIt != pos_.end()) ? GetItemByGuid(posIt->second) : entt::null;
}

uint32_t Bag::GetItemPos(Guid guid)
{
	for (const auto &[pos, mappedGuid] : pos_)
	{
		if (mappedGuid == guid)
		{
			return pos;
		}
	}
	return kInvalidU32Id;
}

uint32_t Bag::HasEnoughSpace(const ItemCountMap &itemsToAdd)
{
	// Dry-run capacity check (no mutation). Walks the request in three steps:
	//   1. Non-stackable items each need their own grid, charged immediately.
	//   2. Stackable items first soak into the free space of existing stacks.
	//   3. Whatever stackable amount is left is charged as new grids.
	auto emptySize = EmptyGridCount();
	ItemCountMap pendingStackItems;
	bool hasStackableItem = false;

	// Step 1: charge non-stackable items, defer stackable ones.
	for (const auto &[configId, count] : itemsToAdd)
	{
		LookupItem(configId);

		if (itemRow->max_stack_size() <= 0)
		{
			LOG_ERROR << "config error:" << configId << " player:" << PlayerGuid();
			return PrintStackAndReturnError(kInvalidTableData);
		}

		if (itemRow->max_stack_size() == 1)
		{
			std::size_t needGridSize = static_cast<std::size_t>(itemRow->max_stack_size() * count);
			if (emptySize <= 0 || emptySize < needGridSize)
			{
				return PrintStackAndReturnError(kBagItemNotStacked);
			}
			emptySize -= needGridSize;
		}
		else
		{
			pendingStackItems.emplace(configId, count);
			hasStackableItem = true;
		}
	}

	if (!hasStackableItem)
	{
		return kSuccess;
	}

	// Step 2: soak stackable demand into the free space of existing stacks.
	for (const auto &[_, item] : itemRegistry_.view<ItemComp>().each())
	{
		for (auto &[stackConfigId, stackCount] : pendingStackItems)
		{
			if (item.config_id() != stackConfigId)
			{
				continue;
			}

			LookupItemOrContinue(stackConfigId);
			auto remainStackSize = itemRow->max_stack_size() - item.size();
			if (remainStackSize <= 0)
			{
				continue;
			}

			if (stackCount <= remainStackSize)
			{
				pendingStackItems.erase(stackConfigId);
				break;
			}

			stackCount -= remainStackSize;
		}
	}

	// Step 3: charge the leftover stackable demand as fresh grids.
	for (const auto &[configId, count] : pendingStackItems)
	{
		LookupItemOrContinue(configId);
		auto needGridSize = CalculateStackGridSize(count, itemRow->max_stack_size());
		if (emptySize <= 0 || emptySize < needGridSize)
		{
			return PrintStackAndReturnError(kBagItemNotStacked);
		}
		emptySize -= needGridSize;
	}

	return kSuccess;
}

uint32_t Bag::HasSufficientItems(const ItemCountMap &requiredItems)
{
	auto itemsToCheck = requiredItems;

	for (const auto &[entity, item] : itemRegistry_.view<ItemComp>().each())
	{
		auto configId = item.config_id();
		auto it = itemsToCheck.find(configId);
		if (it != itemsToCheck.end())
		{
			if (item.size() >= it->second)
			{
				itemsToCheck.erase(it);
			}
			else
			{
				it->second -= item.size();
			}
		}
	}

	return itemsToCheck.empty() ? kSuccess : PrintStackAndReturnError(kBagInsufficientItems);
}

uint32_t Bag::RemoveItems(const ItemCountMap &itemsToRemove)
{
	RETURN_ON_ERROR(HasSufficientItems(itemsToRemove));

	auto remainingToRemove = itemsToRemove;

	// Walk every stack, decrementing the still-needed counts. Slots that
	// reach size 0 are intentionally kept as empty grids (see DelItem test);
	// a later AddStackableItem can refill them.
	for (const auto &[entity, item] : itemRegistry_.view<ItemComp>().each())
	{
		for (auto &[removeConfigId, removeCount] : remainingToRemove)
		{
			if (item.config_id() != removeConfigId)
			{
				continue;
			}

			const auto currentSize = item.size();
			if (removeCount <= currentSize)
			{
				item.set_size(currentSize - removeCount);
				remainingToRemove.erase(removeConfigId);
				break;
			}

			// This stack is fully consumed; keep looking for more.
			removeCount -= currentSize;
			item.set_size(0);
		}

		if (remainingToRemove.empty())
		{
			break;
		}
	}

	return kSuccess;
}

uint32_t Bag::RemoveItemByPos(const RemoveItemByPosParam &param)
{
	if (param.size_ <= 0)
	{
		return PrintStackAndReturnError(kBagDelItemSize);
	}

	auto posIt = pos_.find(param.pos_);
	if (posIt == pos_.end())
	{
		return PrintStackAndReturnError(kBagDelItemPos);
	}

	if (posIt->second != param.item_guid_)
	{
		return PrintStackAndReturnError(kBagDelItemGuid);
	}

	auto guidIt = items_.find(param.item_guid_);
	if (guidIt == items_.end())
	{
		return PrintStackAndReturnError(kBagDelItemFindItem);
	}

	auto &item = itemRegistry_.get<ItemComp>(guidIt->second);
	if (item.config_id() != param.item_config_id_)
	{
		return PrintStackAndReturnError(kBagDelItemConfig);
	}

	if (item.size() < param.size_)
	{
		return PrintStackAndReturnError(kBagItemDeletionSizeMismatch);
	}

	item.set_size(item.size() - param.size_);
	return kSuccess;
}

void Bag::Neaten()
{
	// Group partially-filled stackable items by config so they can be merged.
	std::vector<EntityVector> stackableItemGroups;

	for (auto &&[entity, item] : itemRegistry_.view<ItemComp>().each())
	{
		LookupItemOrContinue(item.config_id());

		if (itemRow->max_stack_size() <= 1)
		{
			continue; // non-stackable: nothing to merge
		}

		if (item.size() >= itemRow->max_stack_size())
		{
			continue; // already a full stack
		}

		bool addedToGroup = false;
		for (auto &group : stackableItemGroups)
		{
			auto &groupAnchor = itemRegistry_.get<ItemComp>(*group.begin());
			if (!CanStack(item, groupAnchor))
			{
				continue;
			}

			group.emplace_back(entity);
			addedToGroup = true;
			break;
		}

		if (!addedToGroup)
		{
			stackableItemGroups.emplace_back(EntityVector{entity});
		}
	}

	// Merge each group: refill leading slots to full, empty the rest.
	GuidVector emptiedItemGuids;
	for (auto &group : stackableItemGroups)
	{
		if (group.empty())
		{
			continue;
		}

		auto &groupAnchor = itemRegistry_.get<ItemComp>(*group.begin());
		LookupItemOrContinue(groupAnchor.config_id());

		uint32_t totalStackSize = 0;
		for (auto &entity : group)
		{
			totalStackSize += itemRegistry_.get<ItemComp>(entity).size();
		}

		std::size_t index = 0;
		for (index = 0; index < group.size(); ++index)
		{
			auto &currentItem = itemRegistry_.get<ItemComp>(group[index]);

			if (totalStackSize <= itemRow->max_stack_size())
			{
				currentItem.set_size(totalStackSize);
				++index;
				break;
			}

			currentItem.set_size(itemRow->max_stack_size());
			totalStackSize -= itemRow->max_stack_size();
		}

		for (; index < group.size(); ++index)
		{
			auto &currentItem = itemRegistry_.get<ItemComp>(group[index]);
			currentItem.set_size(0);
			emptiedItemGuids.emplace_back(currentItem.item_id());
		}
	}

	// Drop the now-empty items, then rebuild the position layout.
	for (auto &guid : emptiedItemGuids)
	{
		DestroyItem(guid);
	}

	pos_.clear();

	for (auto &[guid, entity] : items_)
	{
		auto &item = itemRegistry_.get<ItemComp>(entity);
		OnNewGrid(item.item_id());
	}
}

uint32_t Bag::AddNonStackableItem(ItemComp itemProto)
{
	if (IsSpaceInsufficient(itemProto.size()))
	{
		// TODO: overflow to temp bag or mail
		return PrintStackAndReturnError(kBagAddItemBagFull);
	}

	if (itemProto.size() == 1)
	{
		// A pre-assigned guid is honored; otherwise mint a fresh one.
		if (IsInvalidItemGuid(itemProto))
		{
			itemProto.set_item_id(GenerateItemGuid());
		}
		const auto guid = itemProto.item_id();

		if (InsertItemEntity(std::move(itemProto)) == nullptr)
		{
			LOG_ERROR << "AddNonStackableItem: duplicate guid " << guid << " player " << PlayerGuid();
			return PrintStackAndReturnError(kBagDeleteItemAlreadyHasGuid);
		}

		OnNewGrid(guid);
	}
	else
	{
		// TODO: equipment list with unique guids per piece
		const uint32_t pieceCount = itemProto.size();
		for (uint32_t i = 0; i < pieceCount; ++i)
		{
			ItemComp piece = itemProto;
			piece.set_size(1);
			piece.set_item_id(GenerateItemGuid());
			const auto guid = piece.item_id();

			if (InsertItemEntity(std::move(piece)) == nullptr)
			{
				LOG_ERROR << "AddNonStackableItem(batch): duplicate guid " << guid << " player " << PlayerGuid();
				return PrintStackAndReturnError(kBagDeleteItemAlreadyHasGuid);
			}

			OnNewGrid(guid);
		}
	}
	return kSuccess;
}

uint32_t Bag::AddStackableItem(ItemComp itemProto, uint32_t maxStackSize)
{
	// Adds a stackable item in four phases:
	//   1. Scan existing stacks of the same config that still have room.
	//   2. Compute how many brand-new grids the leftover would need, and
	//      bail out early if the bag can't fit them.
	//   3. Top up the existing stacks found in phase 1.
	//   4. Spill whatever is left into freshly created grids.
	// Phase 1: find existing stacks with remaining capacity
	EntityVector stackTargets;
	std::size_t remainingToPlace = itemProto.size();
	for (auto &&[entity, item] : itemRegistry_.view<ItemComp>().each())
	{
		if (!CanStack(item, itemProto))
		{
			continue;
		}
		if (item.size() > maxStackSize)
		{
			LOG_ERROR << "AddStackableItem: item.size() " << item.size() << " > maxStackSize " << maxStackSize << " player " << PlayerGuid();
			continue;
		}
		auto remainStackSize = maxStackSize - item.size();
		if (remainStackSize <= 0)
		{
			continue;
		}
		stackTargets.emplace_back(entity);
		if (remainingToPlace > remainStackSize)
		{
			remainingToPlace -= remainStackSize;
		}
		else
		{
			remainingToPlace = 0;
			break;
		}
	}

	// Phase 2: check if remaining items fit in empty grids
	std::size_t newGridCount = 0;
	if (remainingToPlace > 0)
	{
		newGridCount = CalculateStackGridSize(remainingToPlace, maxStackSize);
		if (IsSpaceInsufficient(newGridCount))
		{
			return PrintStackAndReturnError(kBagAddItemBagFull);
		}
	}

	// Phase 3: apply stacking to existing items
	auto remainingToStack = itemProto.size();
	for (auto &entity : stackTargets)
	{
		auto &item = itemRegistry_.get<ItemComp>(entity);
		auto remainStackSize = maxStackSize - item.size();
		if (remainStackSize >= remainingToStack)
		{
			item.set_size(item.size() + remainingToStack);
			remainingToStack = 0;
			break;
		}
		item.set_size(item.size() + remainStackSize);
		remainingToStack -= remainStackSize;
	}

	if (remainingToStack <= 0)
	{
		return kSuccess;
	}

	// Phase 4: place remainder into new grids
	for (size_t i = 0; i < newGridCount; ++i)
	{
		ItemComp piece = itemProto;
		piece.set_item_id(GenerateItemGuid());

		if (maxStackSize >= remainingToStack)
		{
			piece.set_size(remainingToStack);
		}
		else
		{
			piece.set_size(maxStackSize);
			remainingToStack -= maxStackSize;
		}
		const auto guid = piece.item_id();

		if (InsertItemEntity(std::move(piece)) == nullptr)
		{
			LOG_ERROR << "AddStackableItem: duplicate guid " << guid << " player " << PlayerGuid();
			return PrintStackAndReturnError(kBagDeleteItemAlreadyHasGuid);
		}

		OnNewGrid(guid);
	}
	return kSuccess;
}

uint32_t Bag::AddItem(const InitItemParam &initItemParam)
{
	auto itemProto = initItemParam.itemPBComp;
	if (itemProto.config_id() <= 0 || itemProto.size() <= 0)
	{
		LOG_ERROR << "bag add item player:" << PlayerGuid();
		return PrintStackAndReturnError(kBagAddItemInvalidParam);
	}

	LookupItem(itemProto.config_id());

	if (itemRow->max_stack_size() <= 0)
	{
		return PrintStackAndReturnError(kInvalidTableData);
	}

	if (itemRow->max_stack_size() == 1)
	{
		return AddNonStackableItem(std::move(itemProto));
	}

	return AddStackableItem(std::move(itemProto), itemRow->max_stack_size());
}

uint32_t Bag::RemoveItem(Guid del_guid)
{
	if (!items_.contains(del_guid))
	{
		return PrintStackAndReturnError(kBagDeleteItemFindGuid);
	}

	// DestroyItem frees the entity, the items_ entry, and the pos_ slot.
	DestroyItem(del_guid);
	return kSuccess;
}

void Bag::Unlock(std::size_t sz)
{
	capacity_ += sz;
}

Guid Bag::GeneratorItemGuid()
{
	return tlsSnowflakeManager.GenerateItemGuid();
}

Guid Bag::LastGeneratorItemGuid()
{
	return tlsSnowflakeManager.GetLastGeneratedItemGuid();
}

bool Bag::IsInvalidItemGuid(const ItemComp &item) const
{
	return item.item_id() == kInvalidGuid || item.item_id() <= 0;
}

void Bag::DestroyItem(Guid guid)
{
	auto guidIt = items_.find(guid);
	if (guidIt == items_.end())
	{
		return;
	}
	// Destroy the backing ECS entity too — otherwise the size-0 component
	// lingers in itemRegistry_ as an orphan that view<ItemComp> still sees,
	// letting a later AddStackableItem stack into an item no longer in
	// items_ (units would be silently lost).
	if (itemRegistry_.valid(guidIt->second))
	{
		itemRegistry_.destroy(guidIt->second);
	}
	items_.erase(guidIt);
}

std::size_t Bag::CalculateStackGridSize(std::size_t total_size, std::size_t max_stack_size)
{
	return (total_size + max_stack_size - 1) / max_stack_size;
}

uint32_t Bag::OnNewGrid(Guid guid)
{
	const auto gridSize = size();
	for (uint32_t i = 0; i < gridSize; ++i)
	{
		if (pos_.contains(i))
		{
			continue;
		}
		pos_.emplace(i, guid);
		return i;
	}
	return kInvalidU32Id;
}

bool Bag::CanStack(const ItemComp &leftItem, const ItemComp &rightItem)
{
	return leftItem.config_id() == rightItem.config_id();
}

// ── Snapshot/migration restore ────────────────────────────────────────
// Wholesale replace bag contents from a snapshot — used by
// bag_marshal::Unmarshal when receiving a migrating player or
// restoring a rollback snapshot. NOT for gameplay use.
//
// Skips AddItem's anomaly detection / transaction_log / gain-block
// checks because the items were already validated when originally
// added in the source zone (or pre-snapshot state). Re-running those
// checks here would double-count anomaly events and emit duplicate
// TX_SYSTEM_GRANT records.
//
// See cpp/libs/services/scene/player/system/bag_marshal.{h,cpp} for
// the proto bridge and docs/design/cross-zone-readiness-audit.md §3.2
// 件 1 for the cross-zone migration design that motivated this.
void Bag::ResetFromSnapshot()
{
	// Drop every ItemComp the bag holds — itemRegistry_ is a private
	// entt::registry, dropping its entities effectively wipes the bag.
	for (const auto &[guid, entity] : items_)
	{
		if (itemRegistry_.valid(entity))
		{
			itemRegistry_.destroy(entity);
		}
	}
	items_.clear();
	pos_.clear();
	// capacity_ deliberately left alone — SetCapacityForRestore sets it
	// from BagAllData.capacities before InsertItemForRestore calls fire.
}

void Bag::InsertItemForRestore(Guid guid, uint32_t configId, uint32_t stackSize, uint32_t pos)
{
	if (guid == kInvalidGuid)
	{
		LOG_ERROR << "Bag::InsertItemForRestore: refusing invalid guid (configId=" << configId << ")";
		return;
	}
	if (items_.find(guid) != items_.end())
	{
		// Duplicate guid in snapshot — log and skip. Indicates upstream
		// data corruption; better to drop one copy than crash.
		LOG_ERROR << "Bag::InsertItemForRestore: duplicate guid=" << guid
				  << " configId=" << configId << " in snapshot, skipping second copy";
		return;
	}

	// Build the entt entity + ItemComp the same shape AddItem would.
	auto itemEntity = itemRegistry_.create();
	auto &itemComp = itemRegistry_.emplace<ItemComp>(itemEntity);
	itemComp.set_item_id(guid);
	itemComp.set_config_id(configId);
	itemComp.set_size(stackSize);

	items_[guid] = itemEntity;
	// Position is persisted from the source-side bag layout. Don't
	// auto-Neaten — the player expects items to be where they left them.
	if (pos != kInvalidU32Id)
	{
		pos_[pos] = guid;
	}
}
