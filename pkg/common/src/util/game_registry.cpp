#include "game_registry.h"

#include "thread_local/dispatcher_manager.h"
#include "stacktrace_system.h"
#include <thread_local/registry_manager.h>
#include <thread_local/thread_local_entity_container.h>

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");

entt::entity ErrorEntity()
{
    if (tlsThreadLocalEntityContainer.errorEntity == entt::null)
    {
        tlsThreadLocalEntityContainer.errorEntity = tlsRegistryManager.globalRegistry.create();
    }
    return tlsThreadLocalEntityContainer.errorEntity;
}

entt::entity OperatorEntity()
{
    if (tlsThreadLocalEntityContainer.operatorEntity == entt::null)
    {
        tlsThreadLocalEntityContainer.operatorEntity = tlsRegistryManager.globalRegistry.create();
    }
    return tlsThreadLocalEntityContainer.operatorEntity;
}

entt::entity GlobalEntity()
{
    if (tlsThreadLocalEntityContainer.globalEntity == entt::null)
    {
        tlsThreadLocalEntityContainer.globalEntity = tlsRegistryManager.globalRegistry.create();
    }
    return tlsThreadLocalEntityContainer.globalEntity;
}

entt::entity GetGlobalGrpcNodeEntity()
{
    if (tlsThreadLocalEntityContainer.globalGrpcNodeEntity == entt::null)
    {
        tlsThreadLocalEntityContainer.globalGrpcNodeEntity = tlsRegistryManager.nodeGlobalRegistry.create();
    }
    return tlsThreadLocalEntityContainer.globalGrpcNodeEntity;
}

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
