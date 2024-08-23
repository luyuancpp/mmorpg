#pragma once

#include "type_define/type_define.h"
#include "util/game_registry.h"
#include "util/snow_flake.h"
#include "util/node_id_generator.h"
#include "thread_local/storage.h"

#include "proto/logic/component/item_base_comp.pb.h"

extern NodeIdGenerator12BitId g_bag_node_sequence;


class ItemComp
{
public:
	ItemComp();
	~ItemComp();

	ItemComp(const ItemComp&);
	ItemComp(ItemComp&& param) noexcept;
	ItemComp& operator=(const ItemComp&);
	ItemComp& operator=(ItemComp&& param)noexcept;

	inline decltype(auto) Entity() const { return entity; }
	inline decltype(auto) Guid()const { return itembase().item_id(); }
	inline decltype(auto) config_id()const { return itembase().config_id(); }
	inline decltype(auto) size()const { return itembase().size(); }

	void set_size(uint32_t sz) { itembase().set_size(sz); }

private:
	inline const ItemPBComp& itembase()const { return tls.itemRegistry.get<ItemPBComp>(entity); }
	inline ItemPBComp& itembase(){ return tls.itemRegistry.get<ItemPBComp>(entity); }
	entt::entity entity;
};

using ItemRawPtrVector = std::vector<ItemComp*>;

struct CreateItemParam
{
	CreateItemParam() { item_base_db.set_size(1); }
	ItemPBComp item_base_db;
};

ItemComp CreateItem(const CreateItemParam& p);