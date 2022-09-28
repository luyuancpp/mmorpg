#include "game_registry.h"


thread_local entt::registry registry;
static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");

entt::entity& error_entity()
{
	thread_local entt::entity singleton = registry.create();
	return singleton;
}

entt::entity& op_enitty()
{
	thread_local entt::entity singleton = registry.create();
	return singleton;
}

void EnityPtrDeleter(entt::entity* p)
{
	registry.destroy(*p);
	delete p;
}

EntityPtr::EntityPtr()
	: entity_(new entt::entity(registry.create()), EnityPtrDeleter)
{

}

entt::entity& global_entity()
{
	thread_local entt::entity singleton;
	return singleton;
}
