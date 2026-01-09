#include "game_registry.h"

#include <muduo/base/Logging.h>
#include <utils/debug/stacktrace_system.h>

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");


//to check 
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
		return entt::null; // 已存在，无法创建
	}

	entt::entity created = registry.create(id);
	if (created != id) {
		LOG_ERROR << "CreateEntityIfNotExists: Failed to create requested entity id=" << entt::to_integral(id)
			<< ", but created id=" << entt::to_integral(created);
        PrintDefaultStackTrace();
		DestroyEntity(registry, created); // 清理失败创建
		return entt::null;
	}

	return created;
}

entt::entity RecreateEntity(entt::registry& registry, entt::entity id) {
	if (registry.valid(id)) {
		DestroyEntity(registry, id); // 先删除旧实体
	}

	entt::entity created = registry.create(id);
	if (created != id) {
		LOG_ERROR << "ResetEntity: Failed to reset entity id=" << entt::to_integral(id)
			<< ", but created id=" << entt::to_integral(created);
        PrintDefaultStackTrace();
		DestroyEntity(registry, created); // 清理异常创建
		return entt::null;
	}

	return created;
}
