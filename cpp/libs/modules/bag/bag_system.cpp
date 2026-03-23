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
#include "thread_context/redis_manager.h"

Bag::Bag()
    : entity(tlsRegistryManager.itemRegistry.create())
{
}

Bag::~Bag()
{
    DestroyEntity(tlsRegistryManager.itemRegistry, entity);
}


std::size_t Bag::GetItemStackSize(uint32_t config_id) const {
    std::size_t totalSize = 0;
    for (const auto& [entity, item] : itemRegistry.view<ItemComp>().each()) {
        if (item.config_id() == config_id) {
            totalSize += item.size();
        }
    }
    return totalSize;
}

ItemComp* Bag::GetItemBaseByGuid(Guid guid) {
    auto it = items_.find(guid);
    return (it != items_.end()) ? itemRegistry.try_get<ItemComp>(it->second) : nullptr;
}

ItemComp* Bag::GetItemBaseByPos(uint32_t pos) {
    auto it = pos_.find(pos);
    return (it != pos_.end()) ? GetItemBaseByGuid(it->second) : nullptr;
}

entt::entity Bag::GetItemByGuid(Guid guid) {
    auto it = items_.find(guid);
    return (it != items_.end()) ? it->second : entt::null;
}

entt::entity Bag::GetItemByPos(uint32_t pos) {
    auto it = pos_.find(pos);
    return (it != pos_.end()) ? GetItemByGuid(it->second) : entt::null;
}

uint32_t Bag::GetItemPos(Guid guid) {
    for (const auto& pit : pos_) {
        if (pit.second == guid) {
            return pit.first;
        }
    }
    return kInvalidU32Id;
}

uint32_t Bag::HasEnoughSpace(const U32U32UnorderedMap& itemsToAdd) {
    auto emptySize = empty_grid_size();
    U32U32UnorderedMap pendingStackItems;
    bool hasStackableItem = false;

    for (const auto& [configId, count] : itemsToAdd) {
		FetchAndValidateItemTable(configId);

        if (itemTable->max_statck_size() <= 0) {
            LOG_ERROR << "config error:" << configId << " player:" << PlayerGuid();
            return PrintStackAndReturnError(kInvalidTableData);
        }

        if (itemTable->max_statck_size() == 1) {
            std::size_t needGridSize = static_cast<std::size_t>(itemTable->max_statck_size() * count);
            if (emptySize <= 0 || emptySize < needGridSize) {
                return PrintStackAndReturnError(kBagItemNotStacked);
            }
            emptySize -= needGridSize;
        }
        else {
            pendingStackItems.emplace(configId, count);
            hasStackableItem = true;
        }
    }

    if (!hasStackableItem) {
        return kSuccess;
    }

    for (const auto& [_, item] : itemRegistry.view<ItemComp>().each()) {
        for (auto& [stackConfigId, stackCount] : pendingStackItems) {
            if (item.config_id() != stackConfigId) {
                continue;
            }

			FetchItemTableOrContinue(stackConfigId);
            auto remainStackSize = itemTable->max_statck_size() - item.size();
            if (remainStackSize <= 0) {
                continue;
            }

            if (stackCount <= remainStackSize) {
                pendingStackItems.erase(stackConfigId);
                break;
            }

            stackCount -= remainStackSize;
        }
    }

    for (const auto& [configId, count] : pendingStackItems) {
		FetchItemTableOrContinue(configId);
        auto needGridSize = CalculateStackGridSize(count, itemTable->max_statck_size());
        if (emptySize <= 0 || emptySize < needGridSize) {
            return PrintStackAndReturnError(kBagItemNotStacked);
        }
        emptySize -= needGridSize;
    }

    return kSuccess;
}

uint32_t Bag::HasSufficientItems(const U32U32UnorderedMap& requiredItems) {
    auto itemsToCheck = requiredItems;

    for (const auto& [entity, item] : itemRegistry.view<ItemComp>().each()) {
        auto configId = item.config_id();
        auto it = itemsToCheck.find(configId);
        if (it != itemsToCheck.end()) {
            if (item.size() >= it->second) {
                itemsToCheck.erase(it);
            }
            else {
                it->second -= item.size();
            }
        }
    }

    return itemsToCheck.empty() ? kSuccess : PrintStackAndReturnError(kBagInsufficientItems);
}


uint32_t Bag::RemoveItems(const U32U32UnorderedMap& itemsToRemove) {
    RETURN_ON_ERROR(HasSufficientItems(itemsToRemove));

    auto itemsToErase = itemsToRemove;
    EntityVector itemsToRemoveReal;

    for (const auto& [e, item] : itemRegistry.view<ItemComp>().each()) {
        for (auto& tryDeleteItem : itemsToErase) {
            if (item.config_id() != tryDeleteItem.first) {
                continue;
            }

            auto sz = item.size();
            if (tryDeleteItem.second <= sz) {
                item.set_size(sz - tryDeleteItem.second);
                itemsToRemoveReal.emplace_back(e);
                itemsToErase.erase(tryDeleteItem.first);
                break;
            }
            else {
                tryDeleteItem.second -= sz;
                item.set_size(0);
                itemsToRemoveReal.emplace_back(e);
            }
        }

        if (itemsToErase.empty()) {
            break;
        }
    }

    return kSuccess;
}

uint32_t Bag::RemoveItemByPos(const RemoveItemByPosParam& p) {
    if (p.size_ <= 0) {
        return PrintStackAndReturnError(kBagDelItemSize);
    }

    auto pit = pos_.find(p.pos_);
    if (pit == pos_.end()) {
        return PrintStackAndReturnError(kBagDelItemPos);
    }

    if (pit->second != p.item_guid_) {
        return PrintStackAndReturnError(kBagDelItemGuid);
    }

    auto item_it = items_.find(p.item_guid_);
    if (item_it == items_.end()) {
        return PrintStackAndReturnError(kBagDelItemFindItem);
    }

    auto& item = itemRegistry.get_or_emplace<ItemComp>(item_it->second);
    if (item.config_id() != p.item_config_id_) {
        return PrintStackAndReturnError(kBagDelItemConfig);
    }

    if (item.size() < p.size_) {
        return PrintStackAndReturnError(kBagItemDeletionSizeMismatch);
    }

    item.set_size(item.size() - p.size_);
    return kSuccess;
}

void Bag::Neaten()
{
	std::vector<EntityVector> stackableItemGroups; // groups of identical stackable items

	for (auto&& [e, item] : itemRegistry.view<ItemComp>().each())
	{
		FetchItemTableOrContinue(item.config_id());

		if (itemTable->max_statck_size() <= 1)
		{
			continue;
		}

		if (item.size() >= itemTable->max_statck_size())//skip full stacks
		{
			continue;
		}
		bool hasNotSameItem = true;
		for (auto& sameVector : stackableItemGroups)
		{
			auto& itemOther = itemRegistry.get_or_emplace<ItemComp>(*sameVector.begin());
			if (!CanStack(item, itemOther))
			{
				continue;
			}

			sameVector.emplace_back(e);
			hasNotSameItem = false;
			break;
		}

		if (hasNotSameItem)
		{
			stackableItemGroups.emplace_back(EntityVector{e});
		}
	}

	GuidVector clearItemGuidList;
	//begin stacking
	for (auto& itemList : stackableItemGroups)
	{
		if (itemList.empty())
		{
			continue;
		}

		auto& firstItem = itemRegistry.get_or_emplace<ItemComp>(*itemList.begin());

		FetchItemTableOrContinue(firstItem.config_id());
	
		uint32_t totalStackSize = 0;
		for (auto& e : itemList)
		{
			totalStackSize += itemRegistry.get_or_emplace<ItemComp>(e).size();
		}

		std::size_t index = 0;

		for (index = 0; index < itemList.size(); ++index)
		{
			auto currentItemEntity = itemList[index];
			auto currentItem = itemRegistry.get_or_emplace<ItemComp>(currentItemEntity);

			if (totalStackSize <= itemTable->max_statck_size())
			{
				currentItem.set_size(totalStackSize);
				++index;
				break;
			}
			else
			{
				currentItem.set_size(itemTable->max_statck_size());
				totalStackSize -= itemTable->max_statck_size();
			}
		}

		for (; index < itemList.size(); ++index)
		{
			auto currentItemEntity = itemList[index];
			auto currentItem = itemRegistry.get_or_emplace<ItemComp>(currentItemEntity);

			currentItem.set_size(0);

			clearItemGuidList.emplace_back(currentItem.item_id());
		}
	}

	//destroy emptied items and grids
	for (auto& it : clearItemGuidList)
	{
		DestroyItem(it);
	}

	pos_.clear();

	//recalculate item positions
	for (auto& [guid, e] : items_)
	{
		auto& item = itemRegistry.get_or_emplace<ItemComp>(e);
		OnNewGrid(item.item_id());
	}
}

uint32_t Bag::AddItem(const InitItemParam& initItemParam)
{
	auto itemPBCompCopy = initItemParam.itemPBComp;
	if (itemPBCompCopy.config_id() <= 0 || itemPBCompCopy.size() <= 0)
	{
		LOG_ERROR << "bag add item player:" << PlayerGuid();
		return PrintStackAndReturnError(kBagAddItemInvalidParam);
	}

	FetchAndValidateItemTable(itemPBCompCopy.config_id());

	if (itemTable->max_statck_size() <= 0)
	{
		return PrintStackAndReturnError(kInvalidTableData);
	}

	if (itemTable->max_statck_size() == 1)//non-stackable, create new guid per item
	{
		if (IsSpaceInsufficient(itemPBCompCopy.size()))
		{
            // todo temp bag or mail
            return PrintStackAndReturnError(kBagAddItemBagFull);
		}

		if (itemPBCompCopy.size() == 1)//single item
		{
			auto newItem = itemRegistry.create();
			auto& newItemPBComp = itemRegistry.emplace<ItemComp>(newItem, std::move(itemPBCompCopy));

			if (IsInvalidItemGuid(newItemPBComp))
			{
				newItemPBComp.set_item_id(GeneratorItemGuid());
			}

			auto it = items_.emplace(newItemPBComp.item_id(), newItem);
			if (!it.second)
			{
				defer(DestroyEntity(itemRegistry, newItem));
				LOG_ERROR << "bag add item" << PlayerGuid();
				return PrintStackAndReturnError(kBagDeleteItemAlreadyHasGuid);
			}

			OnNewGrid(newItemPBComp.item_id());
		}
		else
		{
			//todo equipment list with unique guids
			for (uint32_t i = 0; i < itemPBCompCopy.size(); ++i)
			{
                auto newItem = itemRegistry.create();
                auto& newItemPBComp = itemRegistry.emplace<ItemComp>(newItem, itemPBCompCopy);

				newItemPBComp.set_size(1);
				newItemPBComp.set_item_id(GeneratorItemGuid());

				auto it = items_.emplace(newItemPBComp.item_id(), newItem);
				if (!it.second)
				{
					LOG_ERROR << "bag add item" << PlayerGuid();
					return PrintStackAndReturnError(kBagDeleteItemAlreadyHasGuid);
				}

				OnNewGrid(newItemPBComp.item_id());
			}
		}		
	}
	else if(itemTable->max_statck_size() > 1)//try stacking onto existing grids
	{
		EntityVector doStackItemList;//existing stackable item entities

		std::size_t checkNeedStackSize = itemPBCompCopy.size();
        for (auto&& [e, item] : itemRegistry.view<ItemComp>().each())
		{
			if (!CanStack(item, itemPBCompCopy))
			{
				continue;
			}
			assert(item.size() <= itemTable->max_statck_size());
			auto remainStackSize = itemTable->max_statck_size() - item.size();	
			if (remainStackSize <= 0)
			{
				continue;
			}
			//stackable, collect target entities
			doStackItemList.emplace_back(e);

			if (checkNeedStackSize > remainStackSize )
			{
				checkNeedStackSize -= remainStackSize;
			}
			else
			{
				checkNeedStackSize = 0;
				break;
			}
		}

		std::size_t needEmptyGridSize = 0;

		//remaining items don't fit, check if enough empty grids
		if (checkNeedStackSize > 0)
		{
			needEmptyGridSize = CalculateStackGridSize(checkNeedStackSize, itemTable->max_statck_size());
			if (IsSpaceInsufficient(needEmptyGridSize))
			{
				return PrintStackAndReturnError(kBagAddItemBagFull);
			}
		}

		//apply stacking to existing items
		auto needStackSize = itemPBCompCopy.size();
		for (auto& e : doStackItemList)
		{
			auto& item = itemRegistry.get_or_emplace<ItemComp>(e);
			auto remain_stack_size = itemTable->max_statck_size() - item.size();
			if (remain_stack_size >= needStackSize)
			{
				item.set_size(item.size() + needStackSize);
				break;
			}
			else
			{
				item.set_size(item.size() + remain_stack_size);
				needStackSize -= remain_stack_size;
			}
		}

		if (needStackSize <= 0)//fully stacked
		{
			return kSuccess;
		}

		//place remainder into new grids
		for (size_t i = 0; i < needEmptyGridSize; ++i)
		{
			InitItemParam p;
            auto newItem = itemRegistry.create();
            auto& newItemPBComp = itemRegistry.emplace<ItemComp>(newItem, itemPBCompCopy);

            newItemPBComp.set_item_id(GeneratorItemGuid());

			if (itemTable->max_statck_size() >= needStackSize)
			{
				newItemPBComp.set_size(needStackSize);
			}
			else
			{
				newItemPBComp.set_size(itemTable->max_statck_size());
				needStackSize -= itemTable->max_statck_size();
			}

            auto it = items_.emplace(newItemPBComp.item_id(), newItem);
            if (!it.second)
            {
                LOG_ERROR << "bag add item" << PlayerGuid();
                return kBagDeleteItemAlreadyHasGuid;
            }

            OnNewGrid(newItemPBComp.item_id());
		}
	}
	return kSuccess;
}

uint32_t Bag::RemoveItem(Guid del_guid)
{
	auto it = items_.find(del_guid);
	if (it == items_.end())
	{
		return PrintStackAndReturnError(kBagDeleteItemFindGuid);
	}
	DestroyItem(del_guid);
	for (auto& pit : pos_)
	{
		if (pit.second != del_guid)
		{
			continue;
		}
		pos_.erase(pit.first);
		break;
	}
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

bool Bag::IsInvalidItemGuid(const ItemComp& item)const
{
	return item.item_id() == kInvalidGuid || item.item_id() <= 0;
}


void Bag::DestroyItem(Guid guid)
{
	items_.erase(guid);
}

std::size_t Bag::CalculateStackGridSize(std::size_t total_size, std::size_t max_statck_size)
{
	return (total_size + max_statck_size - 1) / max_statck_size;
}

void Bag::ToString()
{
	std::string contex;
	for (auto& pos : pos_)
	{
		contex +=  GetItemBaseByGuid(pos.second)->ShortDebugString() + "\n";
	}
	LOG_INFO << contex << "\n\n--------------";
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

bool Bag::CanStack(const ItemComp& litem, const ItemComp& ritem)
{
	return litem.config_id() == ritem.config_id();
}

