#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>
#include "muduo/base/Logging.h"
#include "grpc_client/grpc_call_tag.h"
#include <proto/common/base/node.pb.h>
#include <proto/common/base/common.pb.h>

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace NodeUtils
{
	eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
	entt::registry& GetRegistryForNodeType(uint32_t nodeType);
	std::string GetRegistryName(const entt::registry& registry);
	eNodeType GetRegistryType(const entt::registry& registry);
	bool IsSameNode(const std::string& uuid1, const std::string& uuid2);
	bool IsNodeConnected(uint32_t nodeType, const NodeInfo& info);
};


namespace etcdserverpb {
    void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitEtcdGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleEtcdCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
}

namespace loginpb {
    void SetLoginHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void InitLoginGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleLoginCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
}


void SetIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    etcdserverpb::SetEtcdIfEmptyHandler(handler);

    loginpb::SetLoginIfEmptyHandler(handler);

}

void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    etcdserverpb::SetEtcdHandler(handler);

    loginpb::SetLoginHandler(handler);

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
            if (eNodeType::EtcdNodeService == nodeType) {
                etcdserverpb::HandleEtcdCompletedQueueMessage(registry, e, completeQueueComp, grpcTag);
            }
            else if (eNodeType::LoginNodeService == nodeType) {
                loginpb::HandleLoginCompletedQueueMessage(registry, e, completeQueueComp, grpcTag);
            }
        }
    }
}


void InitGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity){
    auto nodeType = NodeUtils::GetRegistryType(registry);
    registry.emplace<grpc::CompletionQueue>(nodeEntity);
    if (eNodeType::EtcdNodeService == nodeType) {
        etcdserverpb::InitEtcdGrpcNode(channel, registry, nodeEntity);
    }
    else if (eNodeType::LoginNodeService == nodeType) {
        loginpb::InitLoginGrpcNode(channel, registry, nodeEntity);
    }
}
