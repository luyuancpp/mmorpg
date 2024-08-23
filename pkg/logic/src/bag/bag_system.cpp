#include "bag_system.h"

#include <vector>

#include "macros/return_define.h"
#include "pbc/common_error_tip.pb.h"
#include "pbc/bag_error_tip.pb.h"

#include"item_config.h"

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
	for (auto&  it : items_)
	{
		if (it.second.config_id() != config_id)
		{
			continue;
		}
		size_sum += it.second.size();
	}

	return size_sum;
}

entt::entity Bag::FindItemByGuid(Guid guid)
{
	auto it = items_.find(guid);
	if (it == items_.end())
	{
		return entt::null;
	}
	return it->second;
}

entt::entity Bag::FindtemByBos(uint32_t pos)
{
	auto it = pos_.find(pos);
	if (it == pos_.end())
	{
		return  entt::null;
	}
	return FindItemByGuid(it->second);
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
			LOG_ERROR << "config error:" << try_item.first << "player:" << player_guid();
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
		auto need_grid_size = calc_item_need_grid_size(it.second, itemTable->max_statck_size());//满叠加的格子
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
				LOG_ERROR << "config error:" << ji.first << "player:" << player_guid();
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
	ItemRawPtrVector real_del_item;//删除的物品,通知客户端
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
	std::vector<ItemRawPtrVector> sameitemEnttiyMatrix;////每个元素里面存相同的物品列表

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
			sameitemEnttiyMatrix.emplace_back(ItemRawPtrVector{e});//没有相同的直接放到新的物品列表里面
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
		OnNewGrid(item);
	}
}

uint32_t Bag::AddItem(const ItemComp& add_item)
{
	auto p_item_base = tls.itemRegistry.try_get<ItemPBComp>(add_item.Entity());
	if (nullptr == p_item_base)
	{
		return kBagAddItemHasNotBaseComponent;
	}

	auto& item_base_db = *p_item_base;
	if (item_base_db.config_id() <= 0 || item_base_db.size() <= 0)
	{
		LOG_ERROR << "bag add item player:" << player_guid();
		return kBagAddItemInvalidParam;
	}

	auto [itemTable, result] = GetItemTable(item_base_db.config_id());
	if (itemTable == nullptr){
		return result;
	}

	if (itemTable->max_statck_size() <= 0)
	{
		LOG_ERROR << "config error:" << item_base_db.config_id()  << "player:" << player_guid();
		return kInvalidTableData;
	}

	if (itemTable->max_statck_size() == 1)//不可以堆叠直接生成新guid
	{
		if (IsFull())
		{
			return kBagAddItemBagFull;
		}
		if (add_item.size() == 1)//只有一个
		{
			if (item_base_db.item_id() == kInvalidGuid)
			{
				item_base_db.set_item_id(g_bag_node_sequence.Generate());
			}
			auto it = items_.emplace(item_base_db.item_id(), std::move(add_item));
			if (!it.second)
			{
				LOG_ERROR << "bag add item" << player_guid();
				return kBagDeleteItemAlreadyHasGuid;
			}
			OnNewGrid(it.first->second);
		}
		else
		{
			//放到新格子里面
			for (uint32_t i = 0; i < item_base_db.size(); ++i)
			{
				CreateItemParam p;
				auto& item_base_db = p.item_base_db;
				item_base_db.set_config_id(add_item.config_id());
				item_base_db.set_item_id(g_bag_node_sequence.Generate());
				auto new_item = CreateItem(p);
				auto it = items_.emplace(item_base_db.item_id(), std::move(new_item));
				if (!it.second)
				{
					LOG_ERROR << "bag add item" << player_guid();
					return kBagDeleteItemAlreadyHasGuid;
				}
				OnNewGrid(it.first->second);
			}
		}		
	}
	else if(itemTable->max_statck_size() > 1)//尝试堆叠到旧格子上
	{
		std::vector<ItemComp*> can_stack;//原来可以叠加的物品
		std::size_t check_need_stack_size = add_item.size();
		for (auto& it : items_)
		{
			auto& item = it.second;
			if (!CanStack(item, add_item))//堆叠判断
			{
				continue;
			}
			assert(itemTable->max_statck_size() >= item.size());
			auto remain_stack_size = itemTable->max_statck_size() - item.size();	
			if (remain_stack_size <= 0)
			{
				continue;
			}
			//可以叠加,先把叠加的物品放进去
			can_stack.emplace_back(&item);
			if (check_need_stack_size > remain_stack_size )
			{
				check_need_stack_size -= remain_stack_size;
			}
			else
			{
				check_need_stack_size = 0;//能放完
				break;
			}
		}
		std::size_t need_grid_size = 0;
		//不可以放完继续检测,先检测格子够不够放，不够放就不行了
		if (check_need_stack_size > 0)
		{
			need_grid_size = calc_item_need_grid_size(check_need_stack_size, itemTable->max_statck_size());//放不完的还需要多少个格子
			if (NotAdequateSize(need_grid_size))
			{
				return kBagAddItemBagFull;
			}
		}
		//检测完毕真正放叠加物品在这里
		//叠加到物品里面
		auto need_stack_size = add_item.size();
		for (auto& it : can_stack)
		{
			auto& item = *it;
			auto& item_base_db = tls.itemRegistry.get<ItemPBComp>(it->Entity());
			auto remain_stack_size = itemTable->max_statck_size() - item.size();
			if (remain_stack_size >= need_stack_size)
			{
				item_base_db.set_size(item_base_db.size() + need_stack_size);
				break;//可以放完了跳出循环，效率会高一点，不用遍历后面的了
			}
			else
			{
				item_base_db.set_size(item_base_db.size() + remain_stack_size);
				need_stack_size -= remain_stack_size;
			}
		}

		if (need_stack_size <= 0)//可以放完
		{
			return kOK;
		}

		//放到新格子里面
		for (size_t i = 0; i < need_grid_size; ++i)
		{
			CreateItemParam p;
			auto& item_base_db = p.item_base_db;
			item_base_db.set_config_id(add_item.config_id());
			item_base_db.set_item_id(g_bag_node_sequence.Generate());
			if (itemTable->max_statck_size() >= need_stack_size)
			{
				item_base_db.set_size(need_stack_size);
			}
			else
			{
				item_base_db.set_size(itemTable->max_statck_size());
				need_stack_size -= itemTable->max_statck_size();
			}
			auto new_item = CreateItem(p);
			auto it = items_.emplace(item_base_db.item_id(), std::move(new_item));
			if (!it.second)
			{
				LOG_ERROR << "bag add item" << player_guid();
				continue;
			}
			OnNewGrid(it.first->second);
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

void Bag::DestroyItem(Guid guid)
{
	items_.erase(guid);
}

std::size_t Bag::calc_item_need_grid_size(std::size_t item_size, std::size_t stack_size)
{
	if (stack_size <= 0)
	{
		return UINT64_MAX;
	}
	//物品中可以堆叠的数量,用除法防止溢出,上面判断过大于0了
	auto stack_grid_size = item_size / stack_size;//满叠加的格子
	if (item_size % stack_size > 0)
	{
		stack_grid_size += 1;
	}
	return stack_grid_size;
}

uint32_t Bag::OnNewGrid(const ItemPBComp& item)
{
	const auto grid_size = size();
	for (uint32_t i = 0; i < grid_size; ++i)
	{
		if (pos_.contains(i))
		{
			continue;
		}
		pos_.emplace(i, item.item_id());
		return i;
	}
	return kInvalidU32Id;
}

bool Bag::CanStack(const ItemPBComp& litem, const ItemPBComp& ritem)
{
	return litem.config_id() == ritem.config_id();
}
