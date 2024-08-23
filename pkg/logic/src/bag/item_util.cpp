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

ItemComp::~ItemComp()
{
	Destroy(tls.itemRegistry, entity);
}
