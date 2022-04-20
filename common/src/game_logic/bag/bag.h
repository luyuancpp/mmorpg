#pragma once

#include <unordered_map>

#include "src/game_logic/bag/item.h"


using ItemsMap = std::unordered_map<common::Guid, Item>;
using PosMap = std::unordered_map<uint32_t, common::Guid>;



struct BagCapacity
{
	static const std::size_t kDefualtCapacity{ 10 };
	std::size_t current_size_{ kDefualtCapacity };
};

class Bag 
{
public:
	Bag();
	using BagEntity = ItemEntity;
	entt::entity entity()const { return entity_.entity(); }
	common::Guid player_guid() { return item_reg.get<common::Guid>(entity()); }
	inline std::size_t size() const { return item_reg.get<BagCapacity>(entity()).current_size_; }
	inline std::size_t item_size() const { return items_.size(); }
	inline std::size_t pos_size() const { return pos_.size(); }

	inline void set_player(common::Guid guid) { item_reg.emplace<common::Guid>(entity(), guid); }

	Item* GetItemByGuid(common::Guid guid);
	Item* GetItemByBos(uint32_t pos);

	inline bool HasItem(common::Guid guid)const { return items_.find(guid) != items_.end();	}
	inline bool AdequateSize(std::size_t s) const { return size() - items_.size() >= s; }//足够空格子
	inline bool NotAdequateSize(std::size_t s) const { return size() - items_.size() < s; }//足够空格子

	uint32_t AddItem(const Item&  add_item);
	uint32_t DelItem(common::Guid del_guid);
	
private:
	void OnNewGrid(const Item& item);

	BagEntity entity_;
	ItemsMap items_;
	PosMap pos_;
};