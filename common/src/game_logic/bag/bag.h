#pragma once

#include <unordered_map>

#include "src/game_logic/bag/item.h"

using ItemsMap = std::unordered_map<common::Guid, Item>;
using PosMap = std::unordered_map<uint32_t, common::Guid>;

struct BagCapacity
{
	static const std::size_t kDefualtCapacity{ 10 };//����Ĭ�ϴ�С
	static const std::size_t kEquipmentCapacity{ 10 };//װ����Ĭ�ϴ�С
	static const std::size_t kBagMaxCapacity{ 100 };
	static const std::size_t kTempBagMaxCapacity{ 200 };
	static const std::size_t kWarehouseMaxCapacity{ 200 };
	std::size_t size_{ kDefualtCapacity };//��ǰ��������
};

enum EnumBagType : uint32_t
{
	kBag = 0,//��ͨ����
	kWarehouse = 1,//�ֿ�
	kEquipment = 2,//װ��
	kTemporary = 3,//��ʱ����
	kBagMax = 4,//ռλ
};

class Bag 
{
public:
	Bag();
	using BagEntity = ItemEntity;
	entt::entity entity()const { return entity_.entity(); }
	common::Guid player_guid() { return item_reg.get<common::Guid>(entity()); }
	inline std::size_t size() const { return item_reg.get<BagCapacity>(entity()).size_; }
	inline std::size_t item_size() const { return items_.size(); }
	inline std::size_t pos_size() const { return pos_.size(); }
	
	inline void set_player(common::Guid guid) { item_reg.emplace<common::Guid>(entity(), guid); }

	Item* GetItemByGuid(common::Guid guid);
	Item* GetItemByBos(uint32_t pos);
	uint32_t GetItemPos(common::Guid guid);//for test
	
	inline bool HasItem(common::Guid guid)const { return items_.find(guid) != items_.end();	}
	inline bool IsFull()const { return size() >= items_.size(); }
	inline bool AdequateSize(std::size_t s) const { return size() - items_.size() >= s; }//�㹻�ո���
	inline bool NotAdequateSize(std::size_t s) const { return size() - items_.size() < s; }//�㹻�ո���
	uint32_t AdequateSizeAddItem(const common::UInt32UInt32UnorderedMap& try_items);//�㹻�ռ����Ʒ
	uint32_t AdequateItem(const common::UInt32UInt32UnorderedMap& try_items);//�㹻��Ʒ

	uint32_t AddItem(const Item&  add_item);
	uint32_t DelItem(common::Guid del_guid);	
	void Unlock(std::size_t sz);
private:
	inline std::size_t empty_grid_size() const { return items_.size() - size(); }
	void OnNewGrid(const Item& item);
	bool CanStack(const Item& litem, const Item& ritem);

	BagEntity entity_;
	ItemsMap items_;
	PosMap pos_;
	uint32_t type_{};
};