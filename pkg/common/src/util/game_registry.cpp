#include "game_registry.h"

#include "thread_local/storage.h"

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");

//todo 注意这里如果调用的registry的话，这个全局变量应该也要跟着清空
entt::entity ErrorEntity()
{
    if (tls.errorEntity == entt::null)
    {
        tls.errorEntity = tls.globalRegistry.create();
    }
    return tls.errorEntity;
}

entt::entity OperatorEntity()
{
    if (tls.operatorEntity == entt::null)
    {
        tls.operatorEntity = tls.globalRegistry.create();
    }
    return tls.operatorEntity;
}

entt::entity GlobalEntity()
{
    if (tls.globalEntity == entt::null)
    {
        tls.globalEntity = tls.globalRegistry.create();
    }
    return tls.globalEntity;
}

entt::entity GetGlobalGrpcNodeEntity()
{
    if (tls.globalGrpcNodeEntity == entt::null)
    {
        tls.globalGrpcNodeEntity = tls.nodeGlobalRegistry.create();
    }
    return tls.globalGrpcNodeEntity;
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
		Destroy(registry, created); // 清理异常创建
		return entt::null;
	}

	return created;
}
