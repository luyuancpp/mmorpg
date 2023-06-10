#include "game_registry.h"

#include "src/game_logic/thread_local/thread_local_storage.h"

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");

//todo init local thread

entt::entity error_entity()
{
	if (tls.error_entity == entt::null)
	{
		tls.error_entity = tls.registry.create();
	}
	return tls.error_entity;
}

entt::entity op_entity()
{
    if (tls.op_entity == entt::null)
    {
        tls.op_entity = tls.registry.create();
    }
    return tls.op_entity;
}

entt::entity& global_entity()
{
    if (tls.global_entity == entt::null)
    {
        tls.global_entity = tls.registry.create();
    }
    return tls.global_entity;
}
