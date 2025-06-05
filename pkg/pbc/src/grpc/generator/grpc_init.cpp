#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>


#include "Etcd"
#include "LoginService"

void SetHandler(const std::function<void(const grpc::ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    SetEtcdHandler(handler);
    SetLoginServiceHandler(handler);
}

void InitCompletedQueue(entt::registry& registry, entt::entity nodeEntity){

    InitEtcdCompletedQueue(registry, nodeEntity);
    InitLoginServiceCompletedQueue(registry, nodeEntity);
}

void HandleCompletedQueueMessage(entt::registry& registry){

    HandleEtcdCompletedQueueMessage(registry, nodeEntity);
    HandleLoginServiceCompletedQueueMessage(registry, nodeEntity);
}
