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
