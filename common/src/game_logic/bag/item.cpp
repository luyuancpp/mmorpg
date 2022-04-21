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
	auto& item_base = item_reg.emplace<ItemBaseDb>(item.entity(), p.item_base_db);
	return item;
}

