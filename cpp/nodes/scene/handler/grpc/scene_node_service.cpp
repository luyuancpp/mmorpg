#include "scene_node_service.h"
#include <future>
///<<< BEGIN WRITING YOUR CODE
#include "modules/scene/comp/scene_comp.h"
#include "modules/scene/comp/scene_node_comp.h"
#include "proto/common/event/scene_event.pb.h"
#include "thread_context/ecs_context.h"
#include "muduo/base/Logging.h"
///<<< END WRITING YOUR CODE

SceneNodeGrpcImpl::SceneNodeGrpcImpl(muduo::net::EventLoop* loop)
    : loop_(loop)
{
}

void SceneNodeGrpcImpl::HandleCreateScene(const ::CreateSceneRequest* request,
    ::CreateSceneResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    // Idempotent: deduplicate by scene_id.
    {
        auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
        for (auto entity : view)
        {
            const auto &info = view.get<SceneInfoComp>(entity);
            if (info.scene_id() == request->scene_id())
            {
                response->mutable_scene_info()->CopyFrom(info);
                LOG_INFO << "[gRPC] CreateScene: scene_id=" << request->scene_id()
                         << " already exists, returning existing";
                return;
            }
        }
    }

    auto sceneEntity = tlsEcs.sceneRegistry.create();

    auto &sceneInfo = tlsEcs.sceneRegistry.emplace<SceneInfoComp>(sceneEntity);
    sceneInfo.set_scene_confid(request->config_id());
    sceneInfo.set_scene_id(request->scene_id());

    tlsEcs.sceneRegistry.emplace<ScenePlayers>(sceneEntity);

    OnSceneCreated event;
    event.set_entity(entt::to_integral(sceneEntity));
    tlsEcs.dispatcher.trigger(event);

    response->mutable_scene_info()->CopyFrom(sceneInfo);

    LOG_INFO << "[gRPC] CreateScene: created entity=" << entt::to_integral(sceneEntity)
             << " config_id=" << request->config_id()
             << " scene_id=" << sceneInfo.scene_id();
    ///<<< END WRITING YOUR CODE
}

void SceneNodeGrpcImpl::HandleDestroyScene(const ::DestroySceneRequest* request)
{
    ///<<< BEGIN WRITING YOUR CODE
    const auto sceneId = request->scene_id();

    entt::entity targetEntity = entt::null;
    auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
    for (auto entity : view)
    {
        const auto &info = view.get<SceneInfoComp>(entity);
        if (info.scene_id() == sceneId)
        {
            targetEntity = entity;
            break;
        }
    }

    if (targetEntity == entt::null)
    {
        LOG_WARN << "[gRPC] DestroyScene: scene_id=" << sceneId << " not found, idempotent OK";
        return;
    }

    OnSceneDestroyed event;
    event.set_entity(entt::to_integral(targetEntity));
    tlsEcs.dispatcher.trigger(event);

    tlsEcs.sceneRegistry.destroy(targetEntity);

    LOG_INFO << "[gRPC] DestroyScene: destroyed scene_id=" << sceneId;
    ///<<< END WRITING YOUR CODE
}

grpc::Status SceneNodeGrpcImpl::CreateScene(grpc::ServerContext* /*context*/,
    const ::CreateSceneRequest* request,
    ::CreateSceneResponse* response)
{
    std::promise<void> promise;
    auto future = promise.get_future();

    loop_->runInLoop([request, response, &promise]
                     {
        HandleCreateScene(request, response);
        promise.set_value(); });

    future.get();
    return grpc::Status::OK;
}

grpc::Status SceneNodeGrpcImpl::DestroyScene(grpc::ServerContext* /*context*/,
    const ::DestroySceneRequest* request,
    ::Empty* response)
{
    std::promise<void> promise;
    auto future = promise.get_future();

    loop_->runInLoop([request, &promise]
                     {
        HandleDestroyScene(request);
        promise.set_value(); });

    future.get();
    return grpc::Status::OK;
}
