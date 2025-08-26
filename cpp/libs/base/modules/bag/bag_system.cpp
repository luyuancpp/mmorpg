#include "bag_system.h"

#include <vector>

#include "error_handling/error_handling.h"
#include "macros/return_define.h"
#include "proto/table/common_error_tip.pb.h"
#include "proto/table/bag_error_tip.pb.h"
#include "util/snow_flake.h"
#include "table/item_config.h"
#include "util/defer.h"
#include <threading/snow_flake_manager.h>
#include "threading/redis_manager.h"

Bag::Bag()
    : entity(tlsRegistryManager.itemRegistry.create())
{
}

Bag::~Bag()
{
    Destroy(tlsRegistryManager.itemRegistry, entity);
}


std::size_t Bag::GetItemStackSize(uint32_t config_id) const {
    std::size_t totalSize = 0;
    for (const auto& [entity, item] : itemRegistry.view<ItemPBComponent>().each()) {
        if (item.config_id() == config_id) {
            totalSize += item.size();
        }
    }
    return totalSize;
}

ItemPBComponent* Bag::GetItemBaseByGuid(Guid guid) {
    auto it = items_.find(guid);
    return (it != items_.end()) ? itemRegistry.try_get<ItemPBComponent>(it->second) : nullptr;
}

ItemPBComponent* Bag::GetItemBaseByPos(uint32_t pos) {
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

uint32_t Bag::HasEnoughSpace(const U32U32UnorderedMap& try_add_item_map) {
    auto emptySize = empty_grid_size();
    U32U32UnorderedMap need_stack_sizes;
    bool has_stack_item = false;

    for (const auto& try_item : try_add_item_map) {
		FetchAndValidateItemTable(try_item.first);

        if (itemTable->max_statck_size() <= 0) {
            LOG_ERROR << "config error:" << try_item.first << "player:" << PlayerGuid();
            return PrintStackAndReturnError(kInvalidTableData);
        }

        if (itemTable->max_statck_size() == 1) {
            std::size_t need_grid_size = static_cast<std::size_t>(itemTable->max_statck_size() * try_item.second);
            if (emptySize <= 0 || emptySize < need_grid_size) {
                return PrintStackAndReturnError(kBagItemNotStacked);
            }
            emptySize -= need_grid_size;
        }
        else {
            need_stack_sizes.emplace(try_item.first, try_item.second);
            has_stack_item = true;
        }
    }

    if (!has_stack_item) {
        return kSuccess;
    }

    for (const auto& [_, item] : itemRegistry.view<ItemPBComponent>().each()) {
        for (auto& ji : need_stack_sizes) {
            if (item.config_id() != ji.first) {
                continue;
            }

			FetchItemTableOrContinue(ji.first);
            auto remain_stack_size = itemTable->max_statck_size() - item.size();
            if (remain_stack_size <= 0) {
                continue;
            }

            if (ji.second <= remain_stack_size) {
                need_stack_sizes.erase(ji.first);
                break;
            }

            ji.second -= remain_stack_size;
        }
    }

    for (const auto& it : need_stack_sizes) {
		FetchItemTableOrContinue(it.first);
        auto need_grid_size = CalculateStackGridSize(it.second, itemTable->max_statck_size());
        if (emptySize <= 0 || emptySize < need_grid_size) {
            return PrintStackAndReturnError(kBagItemNotStacked);
        }
        emptySize -= need_grid_size;
    }

    return kSuccess;
}

uint32_t Bag::HasSufficientItems(const U32U32UnorderedMap& requiredItems) {
    auto itemsToCheck = requiredItems;

    for (const auto& [entity, item] : itemRegistry.view<ItemPBComponent>().each()) {
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

    for (const auto& [e, item] : itemRegistry.view<ItemPBComponent>().each()) {
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

    auto& item = itemRegistry.get<ItemPBComponent>(item_it->second);
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
	std::vector<EntityVector> sameitemEnttiyMatrix;////每个元素里面存相同的物品列表

	for (auto&& [e, item] : itemRegistry.view<ItemPBComponent>().each())
	{
		FetchItemTableOrContinue(item.config_id());

		if (itemTable->max_statck_size() <= 1)
		{
			continue;
		}

		if (item.size() >= itemTable->max_statck_size())//满的不计算了,包括了不可叠加的
		{
			continue;
		}
		//计算未满的
		bool hasNotSameItem = true;//有没有相同的
		for (auto& sameVector : sameitemEnttiyMatrix)
		{
			//看看是不是和第一个物品一样,一样则放到统计列表
			auto& itemOther = itemRegistry.get<ItemPBComponent>(*sameVector.begin());
			if (!CanStack(item, itemOther))
			{
				continue;
			}

			sameVector.emplace_back(e);//把可以叠加的放进相同物品列表里面
			hasNotSameItem = false;
			break;
		}

		if (hasNotSameItem)
		{
			sameitemEnttiyMatrix.emplace_back(EntityVector{e});//没有相同的直接放到新的物品列表里面
		}
	}

	GuidVector clearItemGuidList;
	//开始叠加
	for (auto& itemList : sameitemEnttiyMatrix)
	{
		if (itemList.empty())
		{
			continue;
		}

		auto& firstItem = itemRegistry.get<ItemPBComponent>(*itemList.begin());

		FetchItemTableOrContinue(firstItem.config_id());
	
		//计算总的，然后用总的放到每个格子里面
		uint32_t totalStackSize = 0;
		for (auto& e : itemList)
		{
			totalStackSize += itemRegistry.get<ItemPBComponent>(e).size();
		}

		std::size_t index = 0;//计算过的物品下标

		for (index = 0; index < itemList.size(); ++index)
		{
			auto currentItemEntity = itemList[index];
			auto currentItem = itemRegistry.get<ItemPBComponent>(currentItemEntity);

			if (totalStackSize <= itemTable->max_statck_size())
			{
				currentItem.set_size(totalStackSize);
				++index;//下标加1，break并没有加
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
			auto currentItem = itemRegistry.get<ItemPBComponent>(currentItemEntity);

			currentItem.set_size(0);//被清空的物品

			clearItemGuidList.emplace_back(currentItem.item_id());
		}
	}

	//清空物品清空格子
	for (auto& it : clearItemGuidList)
	{
		DestroyItem(it);
	}

	pos_.clear();

	//重新计算物品位置
	for (auto& [guid, e] : items_)
	{
		auto& item = itemRegistry.get<ItemPBComponent>(e);
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

	if (itemTable->max_statck_size() == 1)//不可以堆叠直接生成新guid
	{
		if (NotAdequateSize(itemPBCompCopy.size()))
		{
            // todo temp bag or mail
            return PrintStackAndReturnError(kBagAddItemBagFull);
		}

		if (itemPBCompCopy.size() == 1)//只有一个
		{
			auto newItem = itemRegistry.create();
			auto& newItemPBComp = itemRegistry.emplace<ItemPBComponent>(newItem, std::move(itemPBCompCopy));

			if (IsInvalidItemGuid(newItemPBComp))
			{
				newItemPBComp.set_item_id(GeneratorItemGuid());
			}

			auto it = items_.emplace(newItemPBComp.item_id(), newItem);
			if (!it.second)
			{
				defer(Destroy(itemRegistry, newItem));
				LOG_ERROR << "bag add item" << PlayerGuid();
				return PrintStackAndReturnError(kBagDeleteItemAlreadyHasGuid);
			}

			OnNewGrid(newItemPBComp.item_id());
		}
		else
		{
			//todo 放装备列表，装备有一堆自己的guild
			//对于一个，放到新格子里面，占用 itemPBCompCopy.size的格子
			for (uint32_t i = 0; i < itemPBCompCopy.size(); ++i)
			{
                auto newItem = itemRegistry.create();
                auto& newItemPBComp = itemRegistry.emplace<ItemPBComponent>(newItem, itemPBCompCopy);

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
	else if(itemTable->max_statck_size() > 1)//尝试堆叠到旧格子上
	{
		EntityVector doStackItemList;//原来可以叠加的物品实体

		std::size_t checkNeedStackSize = itemPBCompCopy.size();
        for (auto&& [e, item] : itemRegistry.view<ItemPBComponent>().each())
		{
			if (!CanStack(item, itemPBCompCopy))//堆叠判断
			{
				continue;
			}
			assert(item.size() <= itemTable->max_statck_size());
			auto remainStackSize = itemTable->max_statck_size() - item.size();	
			if (remainStackSize <= 0)
			{
				continue;
			}
			//可以叠加,先把叠加的物品放进去
			doStackItemList.emplace_back(e);

			if (checkNeedStackSize > remainStackSize )
			{
				checkNeedStackSize -= remainStackSize;
			}
			else
			{
				checkNeedStackSize = 0;//能放完
				break;
			}
		}

		std::size_t needEmptyGridSize = 0;

		//不可以放完继续检测,先检测格子够不够放，不够放就不行了
		if (checkNeedStackSize > 0)
		{
			//放不完的还需要多少个格子
			needEmptyGridSize = CalculateStackGridSize(checkNeedStackSize, itemTable->max_statck_size());
			if (NotAdequateSize(needEmptyGridSize))
			{
				return PrintStackAndReturnError(kBagAddItemBagFull);
			}
		}

		//检测完毕真正放叠加物品在这里
		//叠加到物品里面
		auto needStackSize = itemPBCompCopy.size();
		for (auto& e : doStackItemList)
		{
			auto& item = itemRegistry.get<ItemPBComponent>(e);
			auto remain_stack_size = itemTable->max_statck_size() - item.size();
			if (remain_stack_size >= needStackSize)
			{
				item.set_size(item.size() + needStackSize);
				break;//可以放完了跳出循环，效率会高一点，不用遍历后面的了
			}
			else
			{
				item.set_size(item.size() + remain_stack_size);
				needStackSize -= remain_stack_size;
			}
		}

		if (needStackSize <= 0)//可以放完
		{
			return kSuccess;
		}

		//放到新格子里面
		for (size_t i = 0; i < needEmptyGridSize; ++i)
		{
			InitItemParam p;
            auto newItem = itemRegistry.create();
            auto& newItemPBComp = itemRegistry.emplace<ItemPBComponent>(newItem, itemPBCompCopy);

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
	tlsSnowflakeManager.lastGeneratorItemGuid = tlsSnowflakeManager.itemIdGenerator.Generate();
	return  tlsSnowflakeManager.lastGeneratorItemGuid;
}


Guid Bag::LastGeneratorItemGuid()
{
	return tlsSnowflakeManager.lastGeneratorItemGuid;
}

bool Bag::IsInvalidItemGuid(const ItemPBComponent& item)const
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
		contex +=  GetItemBaseByGuid(pos.second)->DebugString() + "\n";
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

bool Bag::CanStack(const ItemPBComponent& litem, const ItemPBComponent& ritem)
{
	return litem.config_id() == ritem.config_id();
}
