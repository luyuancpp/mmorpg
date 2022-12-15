#include "item.h"

entt::registry item_reg;
ServerSequence16 g_bag_server_sequence;

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

