#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;


namespace loginpb {
    void SetLoginServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
    void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
}

namespace etcdserverpb {
    void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitEtcdCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
    void HandleEtcdCompletedQueueMessage(entt::registry& registry);
}


void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    loginpb::SetLoginServiceHandler(handler);

    etcdserverpb::SetEtcdHandler(handler);

}

void InitCompletedQueue(entt::registry& registry, entt::entity nodeEntity){

    loginpb::InitLoginServiceCompletedQueue(registry, nodeEntity);

    etcdserverpb::InitEtcdCompletedQueue(registry, nodeEntity);

}

void HandleCompletedQueueMessage(entt::registry& registry){

    loginpb::HandleLoginServiceCompletedQueueMessage(registry);

    etcdserverpb::HandleEtcdCompletedQueueMessage(registry);

}
