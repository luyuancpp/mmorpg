#include "bag_system.h"

#include <vector>

#include "constants/tips_id_constants.h"

#include"item_config.h"

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

Item* Bag::GetItemByGuid(Guid guid)
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
		auto p_conf_item = get_item_conf(try_item.first);
		if (nullptr == p_conf_item)
		{
			return kTableIdInvalid;
		}
		if (p_conf_item->max_statck_size() <= 0)
		{
			LOG_ERROR << "config error:" << try_item.first << "player:" << player_guid();
			return kRetConfigData;
		}
		else if (p_conf_item->max_statck_size() == 1)//不可叠加占用一个格子
		{
			std::size_t need_grid_size = static_cast<std::size_t>(p_conf_item->max_statck_size() * try_item.second);
			if (empty_size <= 0 || empty_size < need_grid_size)
			{
				return kRetBagAdequateAddItemSize;
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

	for (auto& item : items_)
	{
		for (auto& ji : need_stack_sizes)
		{
			if (item.second.config_id() != ji.first)
			{
				continue;
			}
			auto p_conf_item = get_item_conf(ji.first);//前面判断过了
			auto remain_stack_size = p_conf_item->max_statck_size() - item.second.size();
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
		auto p_c_item = get_item_conf(it.first);//前面判断过空了，以及除0
		auto need_grid_size = calc_item_need_grid_size(it.second, p_c_item->max_statck_size());//满叠加的格子
		if (empty_size <= 0 || empty_size < need_grid_size)
		{
			return kRetBagAdequateAddItemSize;
		}
		empty_size -= need_grid_size;
	}
	return kOK;
}

uint32_t Bag::HhasSufficientItems(const U32U32UnorderedMap& adequate_items)
{
	auto stack_item_list = adequate_items;
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
				return kTableIdInvalid;
			}
			if (p_c_item->max_statck_size() <= 0)
			{
				LOG_ERROR << "config error:" << it.first << "player:" << player_guid();
				return kRetConfigData;
			}
			if (ji.second <= it.second.size())
			{
				stack_item_list.erase(ji.first);//该物品叠加成功,从列表删除
				break;
			}
			ji.second -= it.second.size();
		}
		if (stack_item_list.empty())
		{
			return kOK;
		}
	}
	if (!stack_item_list.empty())
	{
		return kRetBagAdequateItem;
	}
	return kOK;
}

uint32_t  Bag::DelItem(const U32U32UnorderedMap& try_del_items)
{
	RET_CHECK_RETURN(HhasSufficientItems(try_del_items));
	auto try_del_items_back = try_del_items;
	ItemRawPtrVector real_del_item;//删除的物品
	for (auto& it : items_)
	{
		for (auto& ji : try_del_items_back)
		{
			if (it.second.config_id() != ji.first)
			{
				continue;
			}
			auto sz = it.second.size();
			if (ji.second <= sz)
			{
				it.second.set_size(sz - ji.second);
				real_del_item.emplace_back(&it.second);
				try_del_items_back.erase(ji.first);//该物品叠加成功,从列表删除
				break;
			}
			else
			{
				ji.second -= sz;
				it.second.set_size(0);
				real_del_item.emplace_back(&it.second);
			}			
		}
		if (try_del_items_back.empty())
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
		return kRetBagDelItemSize;
	}
	auto pit = pos_.find(p.pos_);
	if (pit == pos_.end())
	{
		return kRetBagDelItemPos;
	}
	if (pit->second != p.item_guid_)
	{
		return kRetBagDelItemGuid;
	}
	auto item_it = items_.find(p.item_guid_);
	if (item_it == items_.end())
	{
		return kRetBagDelItemFindItem;
	}
	auto& item = item_it->second;
	if (item.config_id() != p.item_config_id_)
	{
		return kRetBagDelItemConfig;
	}
	auto old_size = item.size();
	if (old_size < p.size_)
	{
		return kRetBagDelItemNotAdequateSize;
	}
	item.set_size(old_size - p.size_);
	return kOK;
}

void Bag::Neaten()
{
	std::vector<ItemRawPtrVector> same_items;////每个元素里面存相同的物品列表
	for (auto& it : items_)
	{
		auto& item = it.second;
		auto p_c_item = get_item_conf(item.config_id());
		if (nullptr == p_c_item)
		{
			LOG_ERROR << "get_item_conf" << player_guid() << " config_id" << item.config_id();
			continue;
		}
		if (item.size() >= p_c_item->max_statck_size())//满的不计算了,包括了不可叠加的
		{
			continue;
		}
		//计算未满的
		bool hasnot_same = true;//有没有相同的
		for (auto& ji : same_items)
		{
			if (!CanStack(item, **ji.begin()))
			{
				continue;
			}
			ji.emplace_back(&item);//把可以叠加的放进相同物品列表里面
			hasnot_same = false;
		}
		if (hasnot_same)
		{
			same_items.emplace_back(ItemRawPtrVector{&item});//没有相同的直接放到新的物品列表里面
		}
	}
	GuidVector clear_item_guids;
	//开始叠加
	for (auto& item : same_items)
	{
		if (item.size() == 1)//只有一个，自然不在叠加的计算之内
		{
			continue;
		}
		auto config_id = (*item.begin())->config_id();
		auto p_c_item = get_item_conf(config_id);//上面判断过了，其他人不要模仿
		uint32_t sz = 0;
		for (auto& ji : item)
		{
			sz += ji->size();
		}
		std::size_t index = 0;//使用了的物品下标
		for (index = 0; index < item.size(); ++index)
		{
			if (p_c_item->max_statck_size() >= sz)
			{
				item[index]->set_size(sz);
				++index;//下标加1，break并没有加
				break;
			}
			else
			{
				item[index]->set_size(p_c_item->max_statck_size());
				sz -= p_c_item->max_statck_size();
			}
		}
		for (; index < item.size(); ++index)
		{
			item[index]->set_size(0);//被清空的物品
			clear_item_guids.emplace_back(item[index]->guid());
		}
	}
	if (clear_item_guids.empty())
	{
		return;//联系整理两次
	}
	//清空物品清空格子
	for (auto& it : clear_item_guids)
	{
		items_.erase(it);
	}
	pos_.clear();
	//重新计算物品
	for (auto& it : items_)
	{
		OnNewGrid(it.second);
	}
}

uint32_t Bag::AddItem(const Item& add_item)
{
	auto p_item_base = tls.itemRegistry.try_get<ItemBaseDb>(add_item.entity());
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
		return kTableIdInvalid;
	}
	if (p_c_item->max_statck_size() <= 0)
	{
		LOG_ERROR << "config error:" << item_base_db.config_id()  << "player:" << player_guid();
		return kRetConfigData;
	}
	if (p_c_item->max_statck_size() == 1)//不可以堆叠直接生成新guid
	{
		if (IsFull())
		{
			return kRetBagAddItemBagFull;
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
				return kRetBagDeleteItemAlreadyHasGuid;
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
					return kRetBagDeleteItemAlreadyHasGuid;
				}
				OnNewGrid(it.first->second);
			}
		}		
	}
	else if(p_c_item->max_statck_size() > 1)//尝试堆叠到旧格子上
	{
		std::vector<Item*> can_stack;//原来可以叠加的物品
		std::size_t check_need_stack_size = add_item.size();
		for (auto& it : items_)
		{
			auto& item = it.second;
			if (!CanStack(item, add_item))//堆叠判断
			{
				continue;
			}
			assert(p_c_item->max_statck_size() >= item.size());
			auto remain_stack_size = p_c_item->max_statck_size() - item.size();	
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
			need_grid_size = calc_item_need_grid_size(check_need_stack_size, p_c_item->max_statck_size());//放不完的还需要多少个格子
			if (NotAdequateSize(need_grid_size))
			{
				return kRetBagAddItemBagFull;
			}
		}
		//检测完毕真正放叠加物品在这里
		//叠加到物品里面
		auto need_stack_size = add_item.size();
		for (auto& it : can_stack)
		{
			auto& item = *it;
			auto& item_base_db = tls.itemRegistry.get<ItemBaseDb>(it->entity());
			auto remain_stack_size = p_c_item->max_statck_size() - item.size();
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
			if (p_c_item->max_statck_size() >= need_stack_size)
			{
				item_base_db.set_size(need_stack_size);
			}
			else
			{
				item_base_db.set_size(p_c_item->max_statck_size());
				need_stack_size -= p_c_item->max_statck_size();
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

uint32_t Bag::DelItem(Guid del_guid)
{
	auto it = items_.find(del_guid);
	if (it == items_.end())
	{
		return kRetBagDeleteItemFindGuid;
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
	return kOK;
}

void Bag::Unlock(std::size_t sz)
{
	capacity_.size_ += sz;
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

uint32_t Bag::OnNewGrid(const Item& item)
{
	const auto grid_size = size();
	for (uint32_t i = 0; i < grid_size; ++i)
	{
		if (pos_.contains(i))
		{
			continue;
		}
		pos_.emplace(i, item.guid());
		return i;
	}
	return kInvalidU32Id;
}

bool Bag::CanStack(const Item& litem, const Item& ritem)
{
	return litem.config_id() == ritem.config_id();
}
