#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>
#include "node/system/node_util.h"
#include "muduo/base/Logging.h"
#include "grpc/grpc_tag.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;


namespace playerlocator {
    void SetPlayerLocatorHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerLocatorIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitPlayerLocatorGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandlePlayerLocatorCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
}

namespace etcdserverpb {
    void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitEtcdGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleEtcdCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
}

namespace loginpb {
    void SetLoginServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitLoginServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleLoginServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
}


void SetIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    playerlocator::SetPlayerLocatorIfEmptyHandler(handler);

    etcdserverpb::SetEtcdIfEmptyHandler(handler);

    loginpb::SetLoginServiceIfEmptyHandler(handler);

}

void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    playerlocator::SetPlayerLocatorHandler(handler);

    etcdserverpb::SetEtcdHandler(handler);

    loginpb::SetLoginServiceHandler(handler);

}

void HandleCompletedQueueMessage(entt::registry& registry){
    auto nodeType = NodeUtils::GetRegistryType(registry);
    auto&& view = registry.view<grpc::CompletionQueue>();
    for (auto&& [e, completeQueueComp] : view.each()) {
        void* got_tag = nullptr;
        bool ok = false;
        gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
        while (completeQueueComp.AsyncNext(&got_tag, &ok, tm) == grpc::CompletionQueue::GOT_EVENT) {
            if (!ok) {
                LOG_ERROR << "RPC failed";
                return;
            }
            GrpcTag* grpcTag(reinterpret_cast<GrpcTag*>(got_tag));
            if (eNodeType::PlayerLocatorNodeService == nodeType) {
                playerlocator::HandlePlayerLocatorCompletedQueueMessage(registry, e, completeQueueComp, grpcTag);
            }
            else if (eNodeType::EtcdNodeService == nodeType) {
                etcdserverpb::HandleEtcdCompletedQueueMessage(registry, e, completeQueueComp, grpcTag);
            }
            else if (eNodeType::LoginNodeService == nodeType) {
                loginpb::HandleLoginServiceCompletedQueueMessage(registry, e, completeQueueComp, grpcTag);
            }
        }
    }
}


void InitGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity){
    auto nodeType = NodeUtils::GetRegistryType(registry);
    registry.emplace<grpc::CompletionQueue>(nodeEntity);
    if (eNodeType::PlayerLocatorNodeService == nodeType) {
        playerlocator::InitPlayerLocatorGrpcNode(channel, registry, nodeEntity);
    }
    else if (eNodeType::EtcdNodeService == nodeType) {
        etcdserverpb::InitEtcdGrpcNode(channel, registry, nodeEntity);
    }
    else if (eNodeType::LoginNodeService == nodeType) {
        loginpb::InitLoginServiceGrpcNode(channel, registry, nodeEntity);
    }
}
