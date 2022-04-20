#include "item.h"

thread_local entt::registry item_reg;
thread_local common::ServerSequence g_server_sequence;

using namespace common;

void BagEnityPtrDeleter(entt::entity* p)
{
	item_reg.destroy(*p);
	delete p;
}

ItemEntity::ItemEntity() : entity_(new entt::entity(item_reg.create()), BagEnityPtrDeleter) {}

Item CreateItem(const CreateItemParam& p)
{
	Item item;
	auto& item_base = item_reg.emplace<ItemBaseDb>(item.entity(), ItemBaseDb());
	item_base.set_config_id(p.config_id_);
	item_base.set_size(p.size_);
	assert(item_reg.any_of<ItemBaseDb>(item.entity()));
	return item;
}

Item CreateItem(const ItemBaseDb& pb)
{
	Item item;
	item_reg.emplace<ItemBaseDb>(item.entity(), std::move(pb));
	assert(item_reg.any_of<ItemBaseDb>(item.entity()));
	return item;
}
