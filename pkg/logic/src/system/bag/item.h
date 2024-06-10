#pragma once

#include "src/type_define/type_define.h"
#include "src/util/game_registry.h"
#include "src/util/snow_flake.h"
#include "src/thread_local/thread_local_storage.h"

#include "component_proto/item_base_comp.pb.h"

extern NodeBit16Sequence g_bag_server_sequence;

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
	inline decltype(auto) guid()const { return itembase().item_id(); }
	inline decltype(auto) config_id()const { return itembase().config_id(); }
	inline decltype(auto) size()const { return itembase().size(); }

	void set_size(uint32_t sz) { itembase().set_size(sz); }

private:
	inline const ItemBaseDb& itembase()const { return tls.item_registry.get<ItemBaseDb>(entity()); }
	inline ItemBaseDb& itembase(){ return tls.item_registry.get<ItemBaseDb>(entity()); }
	ItemEntity entity_;
};

using ItemRawPtrVector = std::vector<Item*>;

struct CreateItemParam
{
	CreateItemParam() { item_base_db.set_size(1); }
	ItemBaseDb item_base_db;
};

Item CreateItem(const CreateItemParam& p);