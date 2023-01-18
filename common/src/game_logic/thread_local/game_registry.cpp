#include "game_registry.h"

#include "src/game_logic/thread_local/thread_local_storage.h"

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");

entt::entity& error_entity()
{
	static entt::entity singleton = tls.registry.create();
	return singleton;
}

entt::entity& op_enitty()
{
	static entt::entity singleton = tls.registry.create();
	return singleton;
}

void EnityPtrDeleter(entt::entity* p)
{
	tls.registry.destroy(*p);
	delete p;
}

EntityPtr::EntityPtr()
	: entity_(new entt::entity(tls.registry.create()), EnityPtrDeleter)
{

}

entt::entity& global_entity()
{
	static entt::entity singleton = tls.registry.create();
	return singleton;
}
