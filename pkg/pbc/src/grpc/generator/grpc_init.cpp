#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    SetEtcdHandler(handler);

    void SetLoginServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    SetLoginServiceHandler(handler);

}

void InitCompletedQueue(entt::registry& registry, entt::entity nodeEntity){

    void InitEtcdCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
    InitEtcdCompletedQueue(registry, nodeEntity);

    void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
    InitLoginServiceCompletedQueue(registry, nodeEntity);

}

void HandleCompletedQueueMessage(entt::registry& registry){

    void HandleEtcdCompletedQueueMessage(entt::registry& registry);
    HandleEtcdCompletedQueueMessage(registry);

    void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
    HandleLoginServiceCompletedQueueMessage(registry);

}
