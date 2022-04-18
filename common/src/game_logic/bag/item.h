#pragma once

#include "src/game_logic/game_registry.h"
#include "src/common_type/common_type.h"

#include "src/pb/pbc/component_proto/item_base.pb.h"

extern thread_local entt::registry item_reg;

class ItemEntity
{
public:
	ItemEntity();
	inline entt::entity entity()const { return *entity_.get(); }
private:
	std::shared_ptr<entt::entity> entity_;
};

class Item
{
public:
	Item();
	inline entt::entity entity() const { return entity_.entity(); }
	inline common::Guid guid()const { return itembase().guid(); }
	inline common::Guid config_id()const { return itembase().config_id(); }
	inline decltype(auto) count()const { return itembase().count(); }
private:
	inline const ItemBase& itembase()const { return item_reg.get<ItemBase>(entity()); }
	ItemEntity entity_;
};

Item CreateItem();
Item CreateItem(const ItemBase& pb);