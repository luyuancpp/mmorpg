#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>

void SetHandler(const std::function<void(const grpc::ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
    SetHandler(handler);
}

void InitCompletedQueue(entt::registry& registry, entt::entity nodeEntity){

    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
    InitCompletedQueue(registry, nodeEntity);
}

void HandleCompletedQueueMessage(entt::registry& registry){

    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
    HandleCompletedQueueMessage(registry, nodeEntity);
}
