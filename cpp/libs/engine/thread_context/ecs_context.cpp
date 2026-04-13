#include "ecs_context.h"

thread_local EcsContext tlsEcs;

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
    playerList.clear();
    globalEntity_ = entt::null;
    errorEntity_ = entt::null;
    operatorEntity_ = entt::null;
    grpcNodeEntity_ = entt::null;
}
