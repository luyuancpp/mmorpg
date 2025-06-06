#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>
#include "node/system/node_system.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;


namespace loginpb {
    void SetLoginServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
    void InitLoginServiceStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
}

namespace etcdserverpb {
    void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitEtcdCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
    void InitEtcdStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleEtcdCompletedQueueMessage(entt::registry& registry);
}


void SetIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    loginpb::SetLoginServiceIfEmptyHandler(handler);

    etcdserverpb::SetEtcdIfEmptyHandler(handler);

}

void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    loginpb::SetLoginServiceHandler(handler);

    etcdserverpb::SetEtcdHandler(handler);

}

void InitCompletedQueue(entt::registry& registry, entt::entity nodeEntity){
    auto nodeType = NodeSystem::GetRegistryType(registry);
    if (eNodeType::LoginNodeService == nodeType) {
        loginpb::InitLoginServiceCompletedQueue(registry, nodeEntity);
    }
    else if (eNodeType::EtcdNodeService == nodeType) {
        etcdserverpb::InitEtcdCompletedQueue(registry, nodeEntity);
    }
}

void HandleCompletedQueueMessage(entt::registry& registry){
    auto nodeType = NodeSystem::GetRegistryType(registry);
    if (eNodeType::LoginNodeService == nodeType) {
        loginpb::HandleLoginServiceCompletedQueueMessage(registry);
    }
    else if (eNodeType::EtcdNodeService == nodeType) {
        etcdserverpb::HandleEtcdCompletedQueueMessage(registry);
    }
}


void InitStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity){
    auto nodeType = NodeSystem::GetRegistryType(registry);
    if (eNodeType::LoginNodeService == nodeType) {
        loginpb::InitLoginServiceStub(channel, registry, nodeEntity);
    }
    else if (eNodeType::EtcdNodeService == nodeType) {
        etcdserverpb::InitEtcdStub(channel, registry, nodeEntity);
    }
}
