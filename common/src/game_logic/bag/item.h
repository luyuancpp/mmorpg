#pragma once

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"

#include "src/pb/pbc/component_proto/item_base.pb.h"

extern thread_local entt::registry item_reg;
extern thread_local common::ServerSequence g_server_sequence;

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
	inline decltype(auto) entity() const { return entity_.entity(); }
	inline decltype(auto) guid()const { return itembase().guid(); }
	inline decltype(auto) config_id()const { return itembase().config_id(); }
	inline decltype(auto) size()const { return itembase().size(); }
private:
	inline const ItemBaseDb& itembase()const { return item_reg.get<ItemBaseDb>(entity()); }
	ItemEntity entity_;
};

Item CreateItem();
Item CreateItem(const ItemBaseDb& pb);