#include "game_registry.h"

#include <muduo/base/Logging.h>
#include <util/stacktrace_system.h>

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");


//to check 
void Destroy(entt::registry& registry, entt::entity entity)
{
    if (!registry.valid(entity))
    {
        return;
    }
    registry.destroy(entity);
}

entt::entity TryCreateEntity(entt::registry& registry, entt::entity id) {
	if (registry.valid(id)) {
		return entt::null; // 已存在，无法创建
	}

	entt::entity created = registry.create(id);
	if (created != id) {
		LOG_ERROR << "TryCreateEntity: Failed to create requested entity id=" << entt::to_integral(id)
			<< ", but created id=" << entt::to_integral(created);
        PrintDefaultStackTrace();
		Destroy(registry, created); // 清理失败创建
		return entt::null;
	}

	return created;
}

entt::entity ResetEntity(entt::registry& registry, entt::entity id) {
	if (registry.valid(id)) {
		Destroy(registry, id); // 先删除旧实体
	}

	entt::entity created = registry.create(id);
	if (created != id) {
		LOG_ERROR << "ResetEntity: Failed to reset entity id=" << entt::to_integral(id)
			<< ", but created id=" << entt::to_integral(created);
        PrintDefaultStackTrace();
		Destroy(registry, created); // 清理异常创建
		return entt::null;
	}

	return created;
}
