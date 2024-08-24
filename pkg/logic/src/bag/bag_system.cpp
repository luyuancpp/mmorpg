#include "bag_system.h"

#include <vector>

#include "macros/return_define.h"
#include "pbc/common_error_tip.pb.h"
#include "pbc/bag_error_tip.pb.h"
#include "util/snow_flake.h"
#include "item_config.h"
#include "util/defer.h"

Bag::Bag()
	: entity(tls.itemRegistry.create())
{
}

Bag::~Bag()
{
	Destroy(tls.itemRegistry, entity);
}


std::size_t Bag::GetItemStackSize(uint32_t config_id)const
{
	std::size_t size_sum = 0;
    for (auto&& [_, item] : itemRegistry.view<ItemPBComp>().each())
    {
		if (item.config_id() != config_id)
		{
			continue;
		}
		size_sum += item.size();
	}

	return size_sum;
}

ItemPBComp* Bag::GetItemBaseByGuid(Guid guid)
{
	auto it = items_.find(guid);
	if (it == items_.end())
	{
		return nullptr;
	}

	return itemRegistry.try_get<ItemPBComp>(it->second);
}

ItemPBComp* Bag::GetItemBaseByBos(uint32_t pos)
{
	auto it = pos_.find(pos);
	if (it == pos_.end())
	{
		return  nullptr;
	}
	return GetItemBaseByGuid(it->second);
}

entt::entity Bag::GetItemByGuid(Guid guid)
{
	auto it = items_.find(guid);
	if (it == items_.end())
	{
		return entt::null;
	}

	return it->second;
}

entt::entity Bag::GetItemByBos(uint32_t pos)
{
	auto it = pos_.find(pos);
	if (it == pos_.end())
	{
		return  entt::null;
	}
	return GetItemByGuid(it->second);
}

uint32_t Bag::GetItemPos(Guid guid)
{
	for (auto& pit : pos_)
	{
		if (pit.second == guid)
		{
			return pit.first;
		}
	}
	return kInvalidU32Id;
}

uint32_t Bag::HasEnoughSpace(const U32U32UnorderedMap& try_add_item_map)
{
	auto empty_size = empty_grid_size();
	U32U32UnorderedMap need_stack_sizes;//需要叠加的物品列表
	bool has_stack_item = false;
	//计算不可叠加商品
	for (auto& try_item : try_add_item_map)
	{
		auto [itemTable, result] = GetItemTable(try_item.first);
        if (nullptr == itemTable || result != kOK) {
            return result;
        }

		if (itemTable->max_statck_size() <= 0){
			LOG_ERROR << "config error:" << try_item.first << "player:" << PlayerGuid();
			return kInvalidTableData;
		}
		else if (itemTable->max_statck_size() == 1)//不可叠加占用一个格子
		{
			std::size_t need_grid_size = static_cast<std::size_t>(itemTable->max_statck_size() * try_item.second);
			if (empty_size <= 0 || empty_size < need_grid_size)
			{
				return kBagItemNotStacked;
			}
			empty_size -= need_grid_size;
		}
		else //可以叠加
		{
			need_stack_sizes.emplace(try_item.first, try_item.second);
			has_stack_item = true;
		}
	}

	if (!has_stack_item)//没有需要去背包里面叠加的物品
	{
		return kOK;
	}

	for (auto&& [_, item] : itemRegistry.view<ItemPBComp>().each())
	{
		for (auto& ji : need_stack_sizes)
		{
			if (item.config_id() != ji.first)
			{
				continue;
			}
			auto [itemTable, _] = GetItemTable(ji.first);//前面判断过了
			auto remain_stack_size = itemTable->max_statck_size() - item.size();
			if (remain_stack_size <= 0)//不可以叠加
			{
				continue;
			}
			if (ji.second <= remain_stack_size)
			{
				need_stack_sizes.erase(ji.first);//该物品个数足够,从判断列表删除
				break;
			}
			ji.second -= remain_stack_size;//扣除可以叠加，剩下的个数继续判断
		}		
	}
	//剩下的没叠加成功的
	for (auto& it : need_stack_sizes)
	{
		auto [itemTable, _] = GetItemTable(it.first);//前面判断过空了，以及除0
		auto need_grid_size = CalcItemStackNeedGridSize(it.second, itemTable->max_statck_size());//满叠加的格子
		if (empty_size <= 0 || empty_size < need_grid_size)
		{
			return kBagItemNotStacked;
		}
		empty_size -= need_grid_size;
	}
	return kOK;
}

uint32_t Bag::HasSufficientItems(const U32U32UnorderedMap& adequate_items)
{
	auto stack_item_list = adequate_items;
	for (auto&& [_, item] : itemRegistry.view<ItemPBComp>().each())
	{
		for (auto& ji : stack_item_list)
		{
			if (item.config_id() != ji.first)
			{
				continue;
			}
			auto [itemTable, result] = GetItemTable(ji.first);
			if (nullptr == itemTable)
			{
				return result;
			}
			if (itemTable->max_statck_size() <= 0)
			{
				LOG_ERROR << "config error:" << ji.first << "player:" << PlayerGuid();
				return kInvalidTableData;
			}
			if (ji.second <= item.size())
			{
				stack_item_list.erase(ji.first);//该物品叠加成功,从列表删除
				break;
			}
			ji.second -= item.size();
		}

		if (stack_item_list.empty())
		{
			return kOK;
		}
	}

	if (!stack_item_list.empty())
	{
		return kBagInsufficientItems;
	}

	return kOK;
}

uint32_t  Bag::RemoveItem(const U32U32UnorderedMap& try_del_items)
{
	CHECK_RETURN_IF_NOT_OK(HasSufficientItems(try_del_items));
	auto tryDelItemsCopy = try_del_items;
	EntityVector real_del_item;//删除的物品,通知客户端
	for (auto&& [e, item] : itemRegistry.view<ItemPBComp>().each())
	{
		for (auto& tryDeleteItem : tryDelItemsCopy)
		{
			if (item.config_id() != tryDeleteItem.first)
			{
				continue;
			}
			auto sz = item.size();
			if (tryDeleteItem.second <= sz)
			{
				item.set_size(sz - tryDeleteItem.second);
				real_del_item.emplace_back(e);
				tryDelItemsCopy.erase(tryDeleteItem.first);//该物品叠加成功,从列表删除
				break;
			}
			else
			{
				tryDeleteItem.second -= sz;
				item.set_size(0);
				real_del_item.emplace_back(e);
			}			
		}
		if (tryDelItemsCopy.empty())
		{
			break;
		}
	}
	return kOK;
}

uint32_t Bag::DelItemByPos(const DelItemByPosParam& p)
{
	if (p.size_ <= 0)
	{
		return kBagDelItemSize;
	}
	auto pit = pos_.find(p.pos_);
	if (pit == pos_.end())
	{
		return kBagDelItemPos;
	}
	if (pit->second != p.item_guid_)
	{
		return kBagDelItemGuid;
	}
	auto item_it = items_.find(p.item_guid_);
	if (item_it == items_.end())
	{
		return kBagDelItemFindItem;
	}
	
	auto& item = itemRegistry.get<ItemPBComp>(item_it->second);
	if (item.config_id() != p.item_config_id_)
	{
		return kBagDelItemConfig;
	}

	auto old_size = item.size();
	if (old_size < p.size_)
	{
		return kItemDeletionSizeMismatch;
	}

	item.set_size(old_size - p.size_);
	return kOK;
}

void Bag::Neaten()
{
	std::vector<EntityVector> sameitemEnttiyMatrix;////每个元素里面存相同的物品列表

	for (auto&& [e, item] : itemRegistry.view<ItemPBComp>().each())
	{
		auto [itemTable, result] = GetItemTable(item.config_id());
		if (nullptr == itemTable){
			continue;
		}

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
			auto& itemOther = itemRegistry.get<ItemPBComp>(*sameVector.begin());
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

		auto& firstItem = itemRegistry.get<ItemPBComp>(*itemList.begin());

		auto [itemTable, result] = GetItemTable(firstItem.config_id());
		if (nullptr == itemTable)
		{
			continue;
		}

		//计算总的，然后用总的放到每个格子里面
		uint32_t totalStackSize = 0;
		for (auto& e : itemList)
		{
			totalStackSize += itemRegistry.get<ItemPBComp>(e).size();
		}

		std::size_t index = 0;//计算过的物品下标

		for (index = 0; index < itemList.size(); ++index)
		{
			auto currentItemEntity = itemList[index];
			auto currentItem = itemRegistry.get<ItemPBComp>(currentItemEntity);

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
			auto currentItem = itemRegistry.get<ItemPBComp>(currentItemEntity);

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
		auto& item = itemRegistry.get<ItemPBComp>(e);
		OnNewGrid(item.item_id());
	}
}

uint32_t Bag::AddItem(const InitItemParam& initItemParam)
{
	auto itemPBCompCopy = initItemParam.itemPBComp;
	if (itemPBCompCopy.config_id() <= 0 || itemPBCompCopy.size() <= 0)
	{
		LOG_ERROR << "bag add item player:" << PlayerGuid();
		return kBagAddItemInvalidParam;
	}

	auto [itemTable, result] = GetItemTable(itemPBCompCopy.config_id());
	if (itemTable == nullptr){
		return result;
	}

	if (itemTable->max_statck_size() <= 0)
	{
		return kInvalidTableData;
	}

	if (itemTable->max_statck_size() == 1)//不可以堆叠直接生成新guid
	{
		if (NotAdequateSize(itemPBCompCopy.size()))
		{
            // todo temp bag or mail
            return kBagAddItemBagFull;
		}

		if (itemPBCompCopy.size() == 1)//只有一个
		{
			auto newItem = itemRegistry.create();
			auto& newItemPBComp = itemRegistry.emplace<ItemPBComp>(newItem, std::move(itemPBCompCopy));

			if (IsInvalidItemGuid(newItemPBComp))
			{
				newItemPBComp.set_item_id(GeneratorItemGuid());
			}

			auto it = items_.emplace(newItemPBComp.item_id(), newItem);
			if (!it.second)
			{
				defer(Destroy(itemRegistry, newItem));
				LOG_ERROR << "bag add item" << PlayerGuid();
				return kBagDeleteItemAlreadyHasGuid;
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
                auto& newItemPBComp = itemRegistry.emplace<ItemPBComp>(newItem, itemPBCompCopy);

				newItemPBComp.set_size(1);
				newItemPBComp.set_item_id(GeneratorItemGuid());

				auto it = items_.emplace(newItemPBComp.item_id(), newItem);
				if (!it.second)
				{
					LOG_ERROR << "bag add item" << PlayerGuid();
					return kBagDeleteItemAlreadyHasGuid;
				}

				OnNewGrid(newItemPBComp.item_id());
			}
		}		
	}
	else if(itemTable->max_statck_size() > 1)//尝试堆叠到旧格子上
	{
		EntityVector doStackItemList;//原来可以叠加的物品实体

		std::size_t checkNeedStackSize = itemPBCompCopy.size();
        for (auto&& [e, item] : itemRegistry.view<ItemPBComp>().each())
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
			needEmptyGridSize = CalcItemStackNeedGridSize(checkNeedStackSize, itemTable->max_statck_size());
			if (NotAdequateSize(needEmptyGridSize))
			{
				return kBagAddItemBagFull;
			}
		}

		//检测完毕真正放叠加物品在这里
		//叠加到物品里面
		auto needStackSize = itemPBCompCopy.size();
		for (auto& e : doStackItemList)
		{
			auto& item = itemRegistry.get<ItemPBComp>(e);
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
			return kOK;
		}

		//放到新格子里面
		for (size_t i = 0; i < needEmptyGridSize; ++i)
		{
			InitItemParam p;
            auto newItem = itemRegistry.create();
            auto& newItemPBComp = itemRegistry.emplace<ItemPBComp>(newItem, itemPBCompCopy);

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
	return kOK;
}

uint32_t Bag::RemoveItem(Guid del_guid)
{
	auto it = items_.find(del_guid);
	if (it == items_.end())
	{
		return kBagDeleteItemFindGuid;
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
	return kOK;
}

void Bag::Unlock(std::size_t sz)
{
	capacity_.size_ += sz;
}

Guid Bag::GeneratorItemGuid()
{
	tls.lastGeneratorItemGuid = tls.itemIdGenerator.Generate();
	return  tls.lastGeneratorItemGuid;
}


Guid Bag::LastGeneratorItemGuid()
{
	return tls.lastGeneratorItemGuid;
}

bool Bag::IsInvalidItemGuid(const ItemPBComp& item)
{
	return item.item_id() == kInvalidGuid || item.item_id() <= 0;
}


void Bag::DestroyItem(Guid guid)
{
	items_.erase(guid);
}

std::size_t Bag::CalcItemStackNeedGridSize(std::size_t itemStackSize, std::size_t stackSize)
{
	if (stackSize <= 0)
	{
		return UINT64_MAX;
	}
	//物品中可以堆叠的数量,用除法防止溢出,上面判断过大于0了
	auto stackGridSize = itemStackSize / stackSize;//满叠加的格子
	if (itemStackSize % stackSize > 0)
	{
		stackGridSize += 1;
	}

	return stackGridSize;
}


void Bag::ToString()
{
	std::string contex;
	for (auto& pos : pos_)
	{
		contex +=  GetItemBaseByGuid(pos.second)->DebugString() + "||||||\n";
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

bool Bag::CanStack(const ItemPBComp& litem, const ItemPBComp& ritem)
{
	return litem.config_id() == ritem.config_id();
}
