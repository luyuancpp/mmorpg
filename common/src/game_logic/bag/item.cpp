#include "item.h"

thread_local entt::registry item_reg;

using namespace common;

void BagEnityPtrDeleter(entt::entity* p)
{
	item_reg.destroy(*p);
	delete p;
}

ItemEntity::ItemEntity() : entity_(new entt::entity(item_reg.create()), BagEnityPtrDeleter) {}

Item::Item()
{
	
}

Item CreateItem()
{
	Item item;
	item_reg.emplace<ItemBase>(item.entity(), ItemBase());
	assert(item_reg.any_of<ItemBase>(item.entity()));
	return item;
}

Item CreateItem(const ItemBase& pb)
{
	Item item;
	item_reg.emplace<ItemBase>(item.entity(), pb);
	assert(item_reg.any_of<ItemBase>(item.entity()));
	return item;
}
