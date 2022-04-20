#include "bag.h"

#include <vector>

#include "muduo/base/Logging.h"

#include "src/return_code/error_code.h"

#include"src/game_config/item_config.h"

using namespace common;

Bag::Bag()
{
	item_reg.emplace<BagCapacity>(entity());
}

Item* Bag::GetItemByGuid(common::Guid guid)
{
	auto it = items_.find(guid);
	if (it == items_.end())
	{
		return nullptr;
	}
	return &it->second;
}

Item* Bag::GetItemByBos(uint32_t pos)
{
	auto it = pos_.find(pos);
	if (it == pos_.end())
	{
		return nullptr;
	}
	return GetItemByGuid(it->second);
}

uint32_t Bag::GetItemPos(common::Guid guid)
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

uint32_t Bag::AdequateSizeAddItem(const common::UInt32UInt32UnorderedMap& try_items)
{
	auto empty_size = empty_grid_size();
	UInt32UInt32UnorderedMap stack_item_list;
	bool has_stack_item = false;
	//计算不可叠加商品
	for (auto& it : try_items)
	{
		auto p_c_item = get_item_conf(it.first);
		if (nullptr == p_c_item)
		{
			return RET_TABLE_ID_ERROR;
		}
		if (p_c_item->max_statck_size() <= 0)
		{
			LOG_ERROR << "config error:" << it.first << "player:" << player_guid();
			return kRetCofnigData;
		}
		else if (p_c_item->max_statck_size() == 1)//不可叠加占用一个格子
		{
			if (empty_size <= 0)
			{
				return kRetBagAdequateAddItemSize;
			}
			empty_size -= 1;
		}
		else //可以叠加
		{
			stack_item_list.emplace(it.first, it.second);
			has_stack_item = true;
		}
	}

	if (!has_stack_item)//没有需要去背包里面叠加的物品
	{
		return RET_OK;
	}

	for (auto& it : items_)
	{
		for (auto& ji : stack_item_list)
		{
			if (it.second.config_id() != ji.first)
			{
				continue;
			}
			auto p_c_item = get_item_conf(ji.first);//前面判断过了
			auto remain_stack_size = p_c_item->max_statck_size() - it.second.size();
			if (remain_stack_size <= 0)//不可以叠加
			{
				continue;
			}
			if (ji.second <= remain_stack_size)
			{
				stack_item_list.erase(ji.first);//该物品叠加成功,从列表删除
				break;
			}
			ji.second -= remain_stack_size;
		}		
	}
	std::size_t need_grid_size = 0;
	//剩下的没叠加成功的
	for (auto& it : stack_item_list)
	{
		auto p_c_item = get_item_conf(it.first);//前面判断过空了，以及除0
		auto stack_grid_size =  it.second / p_c_item->max_statck_size();//满叠加的格子
		if (it.second % p_c_item->max_statck_size() > 0)
		{
			stack_grid_size += 1;
		}
		if (empty_size <= 0)
		{
			return kRetBagAdequateAddItemSize;
		}
		empty_size -= stack_grid_size;
	}
	return RET_OK;
}

uint32_t Bag::AdequateItem(const common::UInt32UInt32UnorderedMap& try_items)
{
	auto stack_item_list = try_items;
	for (auto& it : items_)
	{
		for (auto& ji : stack_item_list)
		{
			if (it.second.config_id() != ji.first)
			{
				continue;
			}
			auto p_c_item = get_item_conf(ji.first);
			if (nullptr == p_c_item)
			{
				return RET_TABLE_ID_ERROR;
			}
			if (p_c_item->max_statck_size() <= 0)
			{
				LOG_ERROR << "config error:" << it.first << "player:" << player_guid();
				return kRetCofnigData;
			}
			auto sz = it.second.size();
			if (ji.second <= it.second.size())
			{
				stack_item_list.erase(ji.first);//该物品叠加成功,从列表删除
				break;
			}
			ji.second -= it.second.size();
		}
		if (stack_item_list.empty())
		{
			return RET_OK;
		}
	}
	if (!stack_item_list.empty())
	{
		return kRetBagAdequatetem;
	}
	return RET_OK;
}

uint32_t Bag::AddItem(const Item& add_item)
{
	auto p_item_base = item_reg.try_get<ItemBaseDb>(add_item.entity());
	if (nullptr == p_item_base)
	{
		return kRetBagAddItemHasNotBaseComponent;
	}
	auto& item_base_db = *p_item_base;
	if (item_base_db.config_id() <= 0 || item_base_db.size() <= 0)
	{
		LOG_ERROR << "bag add item player:" << player_guid();
		return kRetBagAddItemInvalidParam;
	}
	auto p_c_item = get_item_conf(item_base_db.config_id());
	if (nullptr == p_c_item)
	{
		return RET_TABLE_ID_ERROR;
	}
	if (p_c_item->max_statck_size() <= 0)
	{
		LOG_ERROR << "config error:" << item_base_db.config_id()  << "player:" << player_guid();
		return kRetCofnigData;
	}
	if (p_c_item->max_statck_size() == 1)//不可以堆叠直接生成新guid
	{
		if (items_.size() >= size())
		{
			return kRetBagAddItemBagFull;
		}
		if (item_base_db.guid() == kInvalidGuid)
		{
			item_base_db.set_guid(g_server_sequence.Generate());
		}
		auto it = items_.emplace(item_base_db.guid(), std::move(add_item));
		if (!it.second)
		{
			LOG_ERROR << "bag add item" << player_guid();
			return kRetBagDeleteItemAlreadyHasGuid;
		}
		OnNewGrid(it.first->second);
	}
	else if(p_c_item->max_statck_size() > 1)//尝试堆叠到旧格子上
	{
		std::vector<Item*> can_stack;//原来可以叠加的物品
		std::size_t check_need_stack_size = add_item.size();
		for (auto& it : items_)
		{
			auto& item = it.second;
			if (item.config_id() != add_item.config_id())//堆叠判断
			{
				continue;
			}
			assert(p_c_item->max_statck_size() >= item.size());
			auto remain_stack_size = p_c_item->max_statck_size() - item.size();	
			if (remain_stack_size > 0)//可以叠加
			{
				can_stack.emplace_back(&item);
			}
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
		//不可以放完继续检测,如果满了就不行了
		if (check_need_stack_size > 0)
		{
			//物品中可以堆叠的数量,用除法防止溢出,上面判断过大于0了
			need_grid_size = check_need_stack_size / p_c_item->max_statck_size();
			if (NotAdequateSize(need_grid_size))
			{
				return kRetBagAddItemBagFull;
			}
		}
		auto need_stack_size = add_item.size();
		for (auto& it : can_stack)
		{
			auto& item = *it;
			auto& item_base_db = item_reg.get<ItemBaseDb>(it->entity());
			auto remain_stack_size = p_c_item->max_statck_size() - item.size();
			if (remain_stack_size >= need_stack_size)
			{
				item_base_db.set_size(item_base_db.size() + need_stack_size);
			}
			else
			{
				item_base_db.set_size(item_base_db.size() + remain_stack_size);
				need_stack_size -= remain_stack_size;
			}
		}
		for (size_t i = 0; i < need_grid_size; ++i)
		{
			ItemBaseDb item_base_db;
			item_base_db.set_config_id(add_item.config_id());
			item_base_db.set_guid(g_server_sequence.Generate());
			if (p_c_item->max_statck_size() >= need_stack_size)
			{
				item_base_db.set_size(need_stack_size);
			}
			else
			{
				item_base_db.set_size(p_c_item->max_statck_size());
				need_stack_size -= p_c_item->max_statck_size();
			}
			auto new_item = CreateItem(item_base_db);
			auto it = items_.emplace(item_base_db.guid(), std::move(new_item));
			if (!it.second)
			{
				LOG_ERROR << "bag add item" << player_guid();
				continue;
			}
			OnNewGrid(it.first->second);
		}
	}
	return RET_OK;
}

uint32_t Bag::DelItem(common::Guid del_guid)
{
	auto it = items_.find(del_guid);
	if (it == items_.end())
	{
		return kRetBagDeleteItemHasnotGuid;
	}
	items_.erase(del_guid);
	for (auto& pit : pos_)
	{
		if (pit.second != del_guid)
		{
			continue;
		}
		pos_.erase(pit.first);
		break;
	}
	return RET_OK;
}

void Bag::Unlock(std::size_t sz)
{
	item_reg.get<BagCapacity>(entity()).size_ += sz;
}

void Bag::OnNewGrid(const Item& item)
{
	uint32_t add_pos = 0;
	uint32_t sz = uint32_t(size());
	for (uint32_t i = 0; i < sz; ++i)
	{
		if (pos_.find(i) != pos_.end())
		{
			continue;
		}
		pos_.emplace(i, item.guid());
		break;
	}
}

bool Bag::CanStack(const Item& litem, const Item& ritem)
{
	return litem.config_id() == ritem.config_id();
}