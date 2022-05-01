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

std::size_t Bag::GetItemStackSize(uint32_t config_id)const
{
	std::size_t sz = 0;
	for (auto&  it : items_)
	{
		if (it.second.config_id() != config_id)
		{
			continue;
		}
		sz += it.second.size();
	}
	return sz;
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

uint32_t Bag::AdequateSizeAddItem(const common::UInt32UInt32UnorderedMap& try_items)
{
	auto empty_size = empty_grid_size();
	UInt32UInt32UnorderedMap stack_item_list;
	bool has_stack_item = false;
	//���㲻�ɵ�����Ʒ
	for (auto& it : try_items)
	{
		auto p_c_item = get_item_conf(it.first);
		if (nullptr == p_c_item)
		{
			return kRetTableId;
		}
		if (p_c_item->max_statck_size() <= 0)
		{
			LOG_ERROR << "config error:" << it.first << "player:" << player_guid();
			return kRetCofnigData;
		}
		else if (p_c_item->max_statck_size() == 1)//���ɵ���ռ��һ������
		{
			std::size_t need_size = static_cast<std::size_t>(p_c_item->max_statck_size() * it.second);
			if (empty_size <= 0 || empty_size < need_size)
			{
				return kRetBagAdequateAddItemSize;
			}
			empty_size -= need_size;
		}
		else //���Ե���
		{
			stack_item_list.emplace(it.first, it.second);
			has_stack_item = true;
		}
	}

	if (!has_stack_item)//û����Ҫȥ����������ӵ���Ʒ
	{
		return kRetOK;
	}

	for (auto& it : items_)
	{
		for (auto& ji : stack_item_list)
		{
			if (it.second.config_id() != ji.first)
			{
				continue;
			}
			auto p_c_item = get_item_conf(ji.first);//ǰ���жϹ���
			auto remain_stack_size = p_c_item->max_statck_size() - it.second.size();
			if (remain_stack_size <= 0)//�����Ե���
			{
				continue;
			}
			if (ji.second <= remain_stack_size)
			{
				stack_item_list.erase(ji.first);//����Ʒ�����㹻,���ж��б�ɾ��
				break;
			}
			ji.second -= remain_stack_size;
		}		
	}
	std::size_t need_grid_size = 0;
	//ʣ�µ�û���ӳɹ���
	for (auto& it : stack_item_list)
	{
		auto p_c_item = get_item_conf(it.first);//ǰ���жϹ����ˣ��Լ���0
		auto stack_grid_size = calc_item_need_grid_size(it.second, p_c_item->max_statck_size());//�����ӵĸ���
		if (empty_size <= 0 || empty_size < stack_grid_size)
		{
			return kRetBagAdequateAddItemSize;
		}
		empty_size -= stack_grid_size;
	}
	return kRetOK;
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
				return kRetTableId;
			}
			if (p_c_item->max_statck_size() <= 0)
			{
				LOG_ERROR << "config error:" << it.first << "player:" << player_guid();
				return kRetCofnigData;
			}
			auto sz = it.second.size();
			if (ji.second <= it.second.size())
			{
				stack_item_list.erase(ji.first);//����Ʒ���ӳɹ�,���б�ɾ��
				break;
			}
			ji.second -= it.second.size();
		}
		if (stack_item_list.empty())
		{
			return kRetOK;
		}
	}
	if (!stack_item_list.empty())
	{
		return kRetBagAdequatetem;
	}
	return kRetOK;
}

uint32_t  Bag::DelItem(const common::UInt32UInt32UnorderedMap& try_del_items)
{
	RET_CHECK_RET(AdequateItem(try_del_items));
	auto del_items = try_del_items;
	IteamRawPtrVector del_item;//ɾ������Ʒ
	for (auto& it : items_)
	{
		for (auto& ji : del_items)
		{
			if (it.second.config_id() != ji.first)
			{
				continue;
			}
			auto sz = it.second.size();
			if (ji.second <= sz)
			{
				it.second.set_size(sz - ji.second);
				del_item.emplace_back(&it.second);
				del_items.erase(ji.first);//����Ʒ���ӳɹ�,���б�ɾ��
				break;
			}
			else
			{
				ji.second -= it.second.size();
				it.second.set_size(0);
				del_item.emplace_back(&it.second);
			}			
		}
		if (del_items.empty())
		{
			return kRetOK;
		}
	}
	return kRetOK;
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
	return kRetOK;
}

void Bag::Neaten()
{
	std::vector<IteamRawPtrVector> same_items;////ÿ��Ԫ���������ͬ����Ʒ�б�
	for (auto& it : items_)
	{
		auto& item = it.second;
		auto p_c_item = get_item_conf(item.config_id());
		if (nullptr == p_c_item)
		{
			LOG_ERROR << "get_item_conf" << player_guid() << " config_id" << item.config_id();
			continue;
		}
		if (p_c_item->max_statck_size() <= 1)
		{
			continue;
		}
		if (item.size() >= p_c_item->max_statck_size())//���Ĳ�������
		{
			continue;
		}
		//����δ����
		bool has_same = false;//��û����ͬ��
		for (auto& ji : same_items)
		{
			assert(!ji.empty());
			if (ji.empty())
			{
				continue;
			}
			if (!CanStack(item, **ji.begin()))
			{
				continue;
			}
			ji.emplace_back(&item);//�ѿ��Ե��ӵķŽ���ͬ��Ʒ����
			has_same = true;
		}
		if (!has_same)
		{
			same_items.emplace_back(IteamRawPtrVector{&item});//û����ͬ��ֱ�ӷŵ��µ���Ʒ�б�����
		}
	}
	GuidVector clear_item_guid;
	//��ʼ����
	for (auto& it : same_items)
	{
		if (it.size() == 1)//ֻ��һ������Ȼ���ڵ��ӵļ���֮��
		{
			continue;
		}
		auto config_id = (*it.begin())->config_id();
		auto p_c_item = get_item_conf(config_id);//�����жϹ��ˣ������˲�Ҫģ��
		if (nullptr == p_c_item)
		{
			LOG_ERROR << "get_item_conf" << player_guid() << " config_id" << config_id;
			continue;
		}
		std::size_t sz = 0;
		for (auto& ji : it)
		{
			sz += ji->size();
		}
		std::size_t need_grid_size = calc_item_need_grid_size(sz, p_c_item->max_statck_size());
		std::size_t index = 0;//ʹ���˵���Ʒ�±�
		for (index = 0; index < it.size(); ++index)
		{
			if (p_c_item->max_statck_size() >= sz)
			{
				it[index]->set_size((uint32_t)sz);
				++index;//�±��1��break��û�м�
				break;
			}
			else
			{
				it[index]->set_size(p_c_item->max_statck_size());
				sz -= p_c_item->max_statck_size();
			}
		}
		for (; index < it.size(); ++index)
		{
			it[index]->set_size(0);//����յ���Ʒ
			clear_item_guid.emplace_back(it[index]->guid());
		}
	}
	if (clear_item_guid.empty())
	{
		return;//��ϵ��������
	}
	//�����Ʒ��ո���
	for (auto& it : clear_item_guid)
	{
		items_.erase(it);
	}
	pos_.clear();
	//���¼�����Ʒ
	for (auto& it : items_)
	{
		OnNewGrid(it.second);
	}
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
		return kRetTableId;
	}
	if (p_c_item->max_statck_size() <= 0)
	{
		LOG_ERROR << "config error:" << item_base_db.config_id()  << "player:" << player_guid();
		return kRetCofnigData;
	}
	if (p_c_item->max_statck_size() == 1)//�����Զѵ�ֱ��������guid
	{
		if (IsFull())
		{
			return kRetBagAddItemBagFull;
		}
		if (add_item.size() == 1)//ֻ��һ��
		{
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
		else
		{
			//�ŵ��¸�������
			for (uint32_t i = 0; i < item_base_db.size(); ++i)
			{
				CreateItemParam p;
				auto& item_base_db = p.item_base_db;
				item_base_db.set_config_id(add_item.config_id());
				item_base_db.set_guid(g_server_sequence.Generate());
				auto new_item = CreateItem(p);
				auto it = items_.emplace(item_base_db.guid(), std::move(new_item));
				if (!it.second)
				{
					LOG_ERROR << "bag add item" << player_guid();
					return kRetBagDeleteItemAlreadyHasGuid;
				}
				OnNewGrid(it.first->second);
			}
		}		
	}
	else if(p_c_item->max_statck_size() > 1)//���Զѵ����ɸ�����
	{
		std::vector<Item*> can_stack;//ԭ�����Ե��ӵ���Ʒ
		std::size_t check_need_stack_size = add_item.size();
		for (auto& it : items_)
		{
			auto& item = it.second;
			if (!CanStack(item, add_item))//�ѵ��ж�
			{
				continue;
			}
			assert(p_c_item->max_statck_size() >= item.size());
			auto remain_stack_size = p_c_item->max_statck_size() - item.size();	
			if (remain_stack_size <= 0)
			{
				continue;
			}
			//���Ե���,�Ȱѵ��ӵ���Ʒ�Ž�ȥ
			can_stack.emplace_back(&item);
			if (check_need_stack_size > remain_stack_size )
			{
				check_need_stack_size -= remain_stack_size;
			}
			else
			{
				check_need_stack_size = 0;//�ܷ���
				break;
			}
		}
		std::size_t need_grid_size = 0;
		//�����Է���������,�ȼ����ӹ������ţ������žͲ�����
		if (check_need_stack_size > 0)
		{
			need_grid_size = calc_item_need_grid_size(check_need_stack_size, p_c_item->max_statck_size());
			if (NotAdequateSize(need_grid_size))
			{
				return kRetBagAddItemBagFull;
			}
		}
		//�����������ŵ�����Ʒ������
		//���ӵ���Ʒ����
		auto need_stack_size = add_item.size();
		for (auto& it : can_stack)
		{
			auto& item = *it;
			auto& item_base_db = item_reg.get<ItemBaseDb>(it->entity());
			auto remain_stack_size = p_c_item->max_statck_size() - item.size();
			if (remain_stack_size >= need_stack_size)
			{
				item_base_db.set_size(item_base_db.size() + need_stack_size);
				break;//���Է���������ѭ����Ч�ʻ��һ�㣬���ñ����������
			}
			else
			{
				item_base_db.set_size(item_base_db.size() + remain_stack_size);
				need_stack_size -= remain_stack_size;
			}
		}
		//�ŵ��¸�������
		for (size_t i = 0; i < need_grid_size; ++i)
		{
			CreateItemParam p;
			auto& item_base_db = p.item_base_db;
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
			auto new_item = CreateItem(p);
			auto it = items_.emplace(item_base_db.guid(), std::move(new_item));
			if (!it.second)
			{
				LOG_ERROR << "bag add item" << player_guid();
				continue;
			}
			OnNewGrid(it.first->second);
		}
	}
	return kRetOK;
}

uint32_t Bag::DelItem(Guid del_guid)
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
	return kRetOK;
}

void Bag::Unlock(std::size_t sz)
{
	item_reg.get<BagCapacity>(entity()).size_ += sz;
}

std::size_t Bag::calc_item_need_grid_size(std::size_t item_size, std::size_t stack_size)
{
	if (stack_size <= 0)
	{
		return UINT64_MAX;
	}
	//��Ʒ�п��Զѵ�������,�ó�����ֹ���,�����жϹ�����0��
	auto stack_grid_size = item_size / stack_size;//�����ӵĸ���
	if (item_size % stack_size > 0)
	{
		stack_grid_size += 1;
	}
	return stack_grid_size;
}

void Bag::OnNewGrid(const Item& item)
{
	if (items_.find(item.guid()) == items_.end())
	{
		LOG_ERROR << "bag add item" << player_guid() << " guid:" << item.guid();
		return;
	}
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
