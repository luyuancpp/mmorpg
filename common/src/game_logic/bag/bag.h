#pragma once

#include <unordered_map>

#include "src/game_logic/bag/item.h"


using ItemsMap = std::unordered_map<common::Guid, Item>;

class Bag 
{
public:
	using BagEntity = ItemEntity;
	entt::entity entity()const { return entity_.entity(); }
	common::Guid player_guid() { return item_reg.get<common::Guid>(entity()); }

	Item* GetItem(common::Guid guid);

	inline bool HasItem(common::Guid guid)const { return items_.find(guid) != items_.end();	}

	uint32_t AddItem(Item&  item);
	uint32_t DelItem(common::Guid guid);
	
private:
	BagEntity entity_;
	ItemsMap items_;
};