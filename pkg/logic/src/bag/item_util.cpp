#include "item_util.h"


NodeIdGenerator12BitId g_bag_node_sequence;

void BagEntityPtrDeleter(entt::entity* p)
{
	Destroy(tls.itemRegistry, *p);
	delete p;
}

ItemEntity::ItemEntity() : entity_(new entt::entity(tls.itemRegistry.create()), BagEntityPtrDeleter) {}

ItemComp CreateItem(const CreateItemParam& p)
{
	ItemComp item;
	tls.itemRegistry.emplace<ItemPBComp>(item.entity(), p.item_base_db);
	return item;
}

