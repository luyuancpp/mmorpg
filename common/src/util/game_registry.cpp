#include "game_registry.h"

#include "src/game_logic/thread_local/thread_local_storage.h"

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");

//todo init local thread

entt::entity error_entity()
{
	static auto singleton = tls.registry.create();
	return singleton;
}

entt::entity op_entity()
{
	static entt::entity singleton = tls.registry.create();
	return singleton;
}

void EntityPtrDeleter(entt::entity* p)
{
	tls.registry.destroy(*p);
	delete p;
}

EntityPtr::EntityPtr()
	: entity_(new entt::entity(tls.registry.create()), EntityPtrDeleter)
{

}

entt::entity& global_entity()
{
	static entt::entity singleton = tls.registry.create();
	return singleton;
}
