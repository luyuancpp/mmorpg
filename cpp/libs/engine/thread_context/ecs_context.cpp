#include "ecs_context.h"

thread_local EcsContext tlsEcs;

EcsContext::EcsContext()
{
    // 让 index 0 / version 0 这个句柄退役,见头文件里的不变量说明。
    actorRegistry.destroy(actorRegistry.create());
}

entt::entity EcsContext::GlobalEntity()
{
    if (globalEntity_ == entt::null) {
        globalEntity_ = globalRegistry.create();
    }
    return globalEntity_;
}

entt::entity EcsContext::ErrorEntity()
{
    if (errorEntity_ == entt::null) {
        errorEntity_ = globalRegistry.create();
    }
    return errorEntity_;
}

entt::entity EcsContext::OperatorEntity()
{
    if (operatorEntity_ == entt::null) {
        operatorEntity_ = globalRegistry.create();
    }
    return operatorEntity_;
}

entt::entity EcsContext::GrpcNodeEntity()
{
    if (grpcNodeEntity_ == entt::null) {
        grpcNodeEntity_ = nodeGlobalRegistry.create();
    }
    return grpcNodeEntity_;
}

entt::entity EcsContext::GetPlayer(Guid playerUid)
{
    auto it = playerList.find(playerUid);
    if (it == playerList.end()) {
        return entt::null;
    }
    return it->second;
}

void EcsContext::Clear()
{
    globalRegistry.clear();
    actorRegistry.clear();
    sceneRegistry.clear();
    itemRegistry.clear();
    nodeGlobalRegistry.clear();
    // 生产代码目前不用 enqueue,这里只是不让排队事件跨过一次 Clear 留到下一轮。
    dispatcher.clear();
    playerList.clear();
    globalEntity_ = entt::null;
    errorEntity_ = entt::null;
    operatorEntity_ = entt::null;
    grpcNodeEntity_ = entt::null;
}
