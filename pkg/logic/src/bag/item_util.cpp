#include "item_util.h"


NodeIdGenerator12BitId g_bag_node_sequence;


ItemComp CreateItem(const CreateItemParam& p)
{
	ItemComp item;
	tls.itemRegistry.emplace<ItemPBComp>(item.Entity(), p.item_base_db);
	return item;
}

ItemComp::ItemComp()
	: entity(tls.itemRegistry.create())
{
}

ItemComp::ItemComp(const ItemComp& param)
{

	entity = tls.itemRegistry.create();
}


ItemComp::ItemComp(ItemComp&& param) noexcept
{
	param.entity = entt::null;
	entity = tls.itemRegistry.create();
}

ItemComp& ItemComp::operator=(const ItemComp&)
{
	entity = tls.itemRegistry.create();
	return *this;

}

ItemComp& ItemComp::operator=(ItemComp&& param)noexcept
{
	param.entity = entt::null;
	entity = tls.itemRegistry.create();
	return *this;
}

ItemComp::~ItemComp()
{
	Destroy(tls.itemRegistry, entity);
}
