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
	// Dry-run capacity check (no mutation): can the bag hold every
	// (config_id -> count) in itemsToAdd, given what it already contains?
	//
	// The request freely mixes two kinds of items:
	//   * Non-stackable (max_stack_size == 1): every unit needs its own grid.
	//   * Stackable: units first top up the free room of existing stacks of the
	//     same config; only the overflow needs brand-new grids.
	//
	// Strategy: figure out how many NEW grids the whole request needs, then
	// compare that single total against the free-grid count once. Two linear
	// passes, no map mutation mid-flight.
	//
	// 只读容量预检(不修改背包):在背包已有内容的前提下,判断能否一次性
	// 放下 itemsToAdd 里的每一项 (config_id -> 数量)。
	// 请求里可以同时混有两类物品:
	//   * 不可叠加 (max_stack_size == 1):每个单位都要单独占一格。
	//   * 可叠加:单位先去填满同 config 现有堆叠的空余,只有溢出部分才占新格。
	// 思路:先算出整批请求总共需要多少个"新格子",最后和空格子数量比一次即可。
	// 两遍线性遍历,中途不修改任何 map。

	// Pass 1 - measure the spare room in existing stacks, per requested config.
	// Walk the bag once (O(items)); configs that aren't being added are skipped,
	// and a non-stackable config never contributes room (its stacks are full at
	// size 1).
	// 第一遍 —— 统计背包中"请求涉及的 config"在现有堆叠里的空余容量。
	// 只遍历背包一次 (O(物品数));没在请求里的 config 直接跳过;不可叠加 config
	// 不会贡献空余(它的堆叠在 size==1 时就已经满了)。
	std::unordered_map<uint32_t, uint32_t> freeRoomByConfig; // config_id -> 现有堆叠累计空余单位数
	for (const auto &[entity, item] : itemRegistry_.view<ItemComp>().each()) // 遍历背包内每一个物品实体
	{
		if (!itemsToAdd.contains(item.config_id())) // 这个物品的 config 不在本次请求里
		{
			continue; // 与本次请求无关,跳过
		}
		LookupItemOrContinue(item.config_id());     // 查物品表(查不到则 continue),注入 itemRow
		const uint32_t maxStack = itemRow->max_stack_size(); // 该 config 的单格最大堆叠上限
		// `<` guards against a corrupt over-full stack underflowing the subtraction.
		// 用 `<` 判断,防止异常的"超满堆叠"在做减法时无符号下溢成巨大值。
		if (item.size() < maxStack) // 这个堆叠还没满,才有空余
		{
			freeRoomByConfig[item.config_id()] += maxStack - item.size(); // 累加该 config 的空余容量
		}
	}

	// Pass 2 - sum the new grids each config still needs after soaking into the
	// free room measured above.
	// 第二遍 —— 每个 config 把数量先吸进上面统计的空余,再累加仍需要的新格子数。
	std::size_t gridsNeeded = 0; // 整批请求总共需要的新格子数
	for (const auto &[configId, count] : itemsToAdd) // 遍历本次请求的每一项
	{
		LookupItem(configId);                        // 查物品表(查不到直接返回错误),注入 itemRow
		const uint32_t maxStack = itemRow->max_stack_size(); // 该 config 的单格最大堆叠上限
		if (maxStack == 0) // 配置非法:堆叠上限不能为 0
		{
			LOG_ERROR << "config error:" << configId << " player:" << PlayerGuid(); // 打错误日志
			return PrintStackAndReturnError(kInvalidTableData); // 返回表数据非法错误
		}

		// How many units fit into existing stacks (always 0 for non-stackable).
		// 能塞进现有堆叠的单位数(不可叠加物品恒为 0)。
		const uint32_t freeRoom = freeRoomByConfig.contains(configId) ? freeRoomByConfig[configId] : 0; // 取该 config 的空余,无则为 0
		const uint32_t overflow = count > freeRoom ? count - freeRoom : 0; // 吸完空余后还剩多少需要新格子

		// The overflow occupies fresh grids, maxStack units per grid (1 per grid
		// for non-stackable since maxStack == 1).
		// 溢出部分占用新格子,每格装 maxStack 个(不可叠加因 maxStack==1 即每格 1 个)。
		gridsNeeded += CalculateStackGridSize(overflow, maxStack); // 折算成向上取整的格子数并累加
	}

	if (gridsNeeded > EmptyGridCount()) // 需要的格子数超过了当前空格子数
	{
		return PrintStackAndReturnError(kBagItemNotStacked); // 放不下,返回空间不足错误
	}
	return kSuccess; // 放得下
}

uint32_t Bag::HasSufficientItems(const ItemCountMap &requiredItems)
{
	// 只读检查:背包里是否拥有 requiredItems 要求的每一项 (config_id -> 需求数量)。
	// 思路:把需求拷一份,遍历背包逐个堆叠去抵扣,需求全部清零即满足。
	auto itemsToCheck = requiredItems; // 拷贝一份需求,边遍历边扣减(不污染入参)

	for (const auto &[entity, item] : itemRegistry_.view<ItemComp>().each()) // 遍历背包内每个物品实体
	{
		auto configId = item.config_id();        // 当前物品的 config
		auto it = itemsToCheck.find(configId);   // 看这个 config 是否在需求清单里
		if (it != itemsToCheck.end())            // 命中需求
		{
			if (item.size() >= it->second)       // 这一堆的数量已经够抵这个 config 的剩余需求
			{
				itemsToCheck.erase(it);          // 该 config 需求已满足,从清单移除
			}
			else                                  // 这一堆不够,只能抵掉一部分
			{
				it->second -= item.size();       // 扣减剩余需求,继续找后面的同 config 堆叠
			}
		}
	}

	// 清单空 => 所有需求都被满足;否则返回物品不足错误。
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

Guid Bag::GenerateItemGuid()
{
	return tlsSnowflakeManager.GenerateItemGuid();
}

Guid Bag::LastGeneratedItemGuid()
{
	return tlsSnowflakeManager.GetLastGeneratedItemGuid();
}

bool Bag::IsInvalidItemGuid(const ItemComp &item) const
{
	return item.item_id() == kInvalidGuid || item.item_id() <= 0;
}

ItemComp *Bag::InsertItemEntity(ItemComp proto)
{
	auto entity = itemRegistry_.create();
	auto &stored = itemRegistry_.emplace<ItemComp>(entity, std::move(proto));

	auto [it, inserted] = items_.emplace(stored.item_id(), entity);
	if (!inserted)
	{
		// Duplicate guid — roll the entity back so items_ and itemRegistry_
		// stay perfectly in sync (the half-created entity must not leak).
		itemRegistry_.destroy(entity);
		return nullptr;
	}
	return &stored;
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

	// Free its grid slot too (pos_ maps pos->guid, so scan for the guid).
	for (auto posIt = pos_.begin(); posIt != pos_.end(); ++posIt)
	{
		if (posIt->second == guid)
		{
			pos_.erase(posIt);
			break;
		}
	}
}

void Bag::ClearAllItems()
{
	for (const auto &[guid, entity] : items_)
	{
		if (itemRegistry_.valid(entity))
		{
			itemRegistry_.destroy(entity);
		}
	}
	items_.clear();
	pos_.clear();
}

std::size_t Bag::CalculateStackGridSize(std::size_t total_size, std::size_t max_stack_size)
{
	return (total_size + max_stack_size - 1) / max_stack_size;
}

uint32_t Bag::OnNewGrid(Guid guid)
{
	const auto gridSize = Capacity();
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
	// ClearAllItems drops every entity + items_ + pos_ entry.
	// capacity_ deliberately left alone — SetCapacityForRestore sets it
	// from BagAllData.capacities before InsertItemForRestore calls fire.
	ClearAllItems();
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

	// Build the ItemComp the same shape AddItem would, then route through
	// the single InsertItemEntity chokepoint so items_/itemRegistry_ stay
	// in sync.
	ItemComp proto;
	proto.set_item_id(guid);
	proto.set_config_id(configId);
	proto.set_size(stackSize);

	if (InsertItemEntity(std::move(proto)) == nullptr)
	{
		LOG_ERROR << "Bag::InsertItemForRestore: duplicate guid=" << guid
				  << " configId=" << configId << ", skipping";
		return;
	}
	// Position is persisted from the source-side bag layout. Don't
	// auto-Neaten — the player expects items to be where they left them.
	if (pos != kInvalidU32Id)
	{
		pos_[pos] = guid;
	}
}
