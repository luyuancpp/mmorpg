#include "scene_grpc_service.h"

#include <future>
#include "modules/scene/comp/scene_comp.h"
#include "modules/scene/comp/scene_node_comp.h"
#include "proto/common/event/scene_event.pb.h"
#include "thread_context/ecs_context.h"
#include "muduo/base/Logging.h"

SceneGrpcServiceImpl::SceneGrpcServiceImpl(muduo::net::EventLoop *loop)
    : loop_(loop)
{
}

grpc::Status SceneGrpcServiceImpl::CreateScene(grpc::ServerContext *context,
                                                const CreateSceneRequest *request,
                                                CreateSceneResponse *response)
{
    if (request->config_id() == 0 || request->scene_id() == 0)
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "config_id and scene_id must be non-zero");
    }

    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    loop_->runInLoop([request, response, &promise]
                     {
        // Idempotent: deduplicate by scene_id.
        {
            auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
            for (auto entity : view)
            {
                const auto &info = view.get<SceneInfoComp>(entity);
                if (info.guid() == request->scene_id())
                {
                    response->mutable_scene_info()->CopyFrom(info);
                    LOG_INFO << "[gRPC] CreateScene: scene_id=" << request->scene_id()
                             << " already exists, returning existing";
                    promise.set_value(grpc::Status::OK);
                    return;
                }
            }
        }

        auto sceneEntity = tlsEcs.sceneRegistry.create();

        auto &sceneInfo = tlsEcs.sceneRegistry.emplace<SceneInfoComp>(sceneEntity);
        sceneInfo.set_scene_confid(request->config_id());
        sceneInfo.set_guid(request->scene_id());

        tlsEcs.sceneRegistry.emplace<ScenePlayers>(sceneEntity);

        OnSceneCreated event;
        event.set_entity(entt::to_integral(sceneEntity));
        tlsEcs.dispatcher.trigger(event);

        response->mutable_scene_info()->CopyFrom(sceneInfo);

        LOG_INFO << "[gRPC] CreateScene: created entity=" << entt::to_integral(sceneEntity)
                 << " config_id=" << request->config_id()
                 << " guid=" << sceneInfo.guid();

        promise.set_value(grpc::Status::OK); });

    return future.get();
}

grpc::Status SceneGrpcServiceImpl::DestroyScene(grpc::ServerContext *context,
                                                 const DestroySceneRequest *request,
                                                 Empty *response)
{
    const auto sceneId = request->scene_id();
    if (sceneId == 0)
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "scene_id must be non-zero");
    }

    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    loop_->runInLoop([sceneId, &promise]
                     {
        entt::entity targetEntity = entt::null;
        auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
        for (auto entity : view)
        {
            const auto &info = view.get<SceneInfoComp>(entity);
            if (info.guid() == sceneId)
            {
                targetEntity = entity;
                break;
            }
        }

        if (targetEntity == entt::null)
        {
            LOG_WARN << "[gRPC] DestroyScene: scene_id=" << sceneId << " not found";
            promise.set_value(grpc::Status(grpc::StatusCode::NOT_FOUND, "scene not found"));
            return;
        }

        OnSceneDestroyed event;
        event.set_entity(entt::to_integral(targetEntity));
        tlsEcs.dispatcher.trigger(event);

        tlsEcs.sceneRegistry.destroy(targetEntity);

        LOG_INFO << "[gRPC] DestroyScene: destroyed scene_id=" << sceneId;
        promise.set_value(grpc::Status::OK); });

    return future.get();
}
