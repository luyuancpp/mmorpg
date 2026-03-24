#include "game_registry.h"

#include <muduo/base/Logging.h>
#include <utils/debug/stacktrace_system.h>

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");


// TODO: review whether validity check is sufficient
void DestroyEntity(entt::registry& registry, entt::entity entity)
{
    if (!registry.valid(entity))
    {
        return;
    }
    registry.destroy(entity);
}

entt::entity CreateEntityIfNotExists(entt::registry& registry, entt::entity id) {
	if (registry.valid(id)) {
		return entt::null; // Already exists
	}

	entt::entity created = registry.create(id);
	if (created != id) {
		LOG_ERROR << "CreateEntityIfNotExists: Failed to create requested entity id=" << entt::to_integral(id)
			<< ", but created id=" << entt::to_integral(created);
        PrintDefaultStackTrace();
		DestroyEntity(registry, created); // Clean up failed creation
		return entt::null;
	}

	return created;
}

entt::entity RecreateEntity(entt::registry& registry, entt::entity id) {
	if (registry.valid(id)) {
		DestroyEntity(registry, id); // Destroy old entity first
	}

	entt::entity created = registry.create(id);
	if (created != id) {
		LOG_ERROR << "ResetEntity: Failed to reset entity id=" << entt::to_integral(id)
			<< ", but created id=" << entt::to_integral(created);
        PrintDefaultStackTrace();
		DestroyEntity(registry, created); // Clean up unexpected entity
		return entt::null;
	}

	return created;
}
