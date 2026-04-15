#pragma once

#include "proto/scene/scene.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <muduo/net/EventLoop.h>

// gRPC server-side implementation for the Scene service.
// Methods are invoked from gRPC thread pool threads, so all ECS work
// is dispatched to the muduo event loop via runInLoop + promise/future.
class SceneGrpcServiceImpl final : public Scene::Service
{
public:
    explicit SceneGrpcServiceImpl(muduo::net::EventLoop *loop);

    grpc::Status CreateScene(grpc::ServerContext *context,
                             const CreateSceneRequest *request,
                             CreateSceneResponse *response) override;

    grpc::Status DestroyScene(grpc::ServerContext *context,
                              const DestroySceneRequest *request,
                              Empty *response) override;

private:
    muduo::net::EventLoop *loop_;
};
