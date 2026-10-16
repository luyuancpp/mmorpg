#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>
#include "core/network/grpc_client_runtime.h"
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
	common::base::eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
	entt::registry& GetRegistryForNodeType(uint32_t nodeType);
	std::string GetRegistryName(const entt::registry& registry);
	common::base::eNodeType GetRegistryType(const entt::registry& registry);
	bool IsSameNode(const std::string& uuid1, const std::string& uuid2);
	bool IsNodeConnected(uint32_t nodeType, const NodeInfo& info);
};

    void SetBattleNodeHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetBattleNodeIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetBattleNodeFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetBattleNodeIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetBattleNodeCallDeadline(std::chrono::milliseconds deadline);
    void InitBattleNodeGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleBattleNodeCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);

    void SetPlayerBattleHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerBattleIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerBattleFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerBattleIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerBattleCallDeadline(std::chrono::milliseconds deadline);
    void InitPlayerBattleGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandlePlayerBattleCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);

namespace chatpb {
    void SetChatHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetChatIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetChatFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetChatIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetChatCallDeadline(std::chrono::milliseconds deadline);
    void InitChatGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleChatCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace client_rpc_router {
    void SetClientRpcRouterHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetClientRpcRouterIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetClientRpcRouterFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetClientRpcRouterIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetClientRpcRouterCallDeadline(std::chrono::milliseconds deadline);
    void InitClientRpcRouterGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleClientRpcRouterCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace data_service {
    void SetDataServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetDataServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetDataServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetDataServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetDataServiceCallDeadline(std::chrono::milliseconds deadline);
    void InitDataServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleDataServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace etcdserverpb {
    void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetEtcdIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetEtcdCallDeadline(std::chrono::milliseconds deadline);
    void InitEtcdGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleEtcdCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace friendpb {
    void SetFriendHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetFriendIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetFriendFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetFriendIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetFriendCallDeadline(std::chrono::milliseconds deadline);
    void InitFriendGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleFriendCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace guildpb {
    void SetGuildHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildCallDeadline(std::chrono::milliseconds deadline);
    void InitGuildGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleGuildCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace guildpb {
    void SetGuildInternalHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildInternalIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildInternalCallDeadline(std::chrono::milliseconds deadline);
    void InitGuildInternalGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleGuildInternalCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace loginpb {
    void SetLoginHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetLoginIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetLoginCallDeadline(std::chrono::milliseconds deadline);
    void InitLoginGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleLoginCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace match {
    void SetMatchInternalHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchInternalIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchInternalCallDeadline(std::chrono::milliseconds deadline);
    void InitMatchInternalGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleMatchInternalCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace match {
    void SetMatchServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchServiceCallDeadline(std::chrono::milliseconds deadline);
    void InitMatchServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleMatchServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace player_inbox {
    void SetPlayerInboxHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxCallDeadline(std::chrono::milliseconds deadline);
    void InitPlayerInboxGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandlePlayerInboxCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace player_inbox {
    void SetPlayerInboxAdminHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxAdminIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxAdminCallDeadline(std::chrono::milliseconds deadline);
    void InitPlayerInboxAdminGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandlePlayerInboxAdminCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace scene_manager {
    void SetSceneManagerServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneManagerServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneManagerServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneManagerServiceCallDeadline(std::chrono::milliseconds deadline);
    void InitSceneManagerServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleSceneManagerServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace scene_node {
    void SetSceneNodeServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneNodeServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneNodeServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneNodeServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneNodeServiceCallDeadline(std::chrono::milliseconds deadline);
    void InitSceneNodeServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleSceneNodeServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace teampb {
    void SetTeamHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTeamIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTeamFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTeamIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTeamCallDeadline(std::chrono::milliseconds deadline);
    void InitTeamGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleTeamCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace trade {
    void SetJubaozhaiHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetJubaozhaiIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetJubaozhaiIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline);
    void InitJubaozhaiGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleJubaozhaiCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

namespace trade {
    void SetTradeAdminHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTradeAdminIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTradeAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTradeAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTradeAdminCallDeadline(std::chrono::milliseconds deadline);
    void InitTradeAdminGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
    void HandleTradeAdminCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
}

void SetIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    ::SetBattleNodeIfEmptyHandler(handler);

    ::SetPlayerBattleIfEmptyHandler(handler);

    chatpb::SetChatIfEmptyHandler(handler);

    client_rpc_router::SetClientRpcRouterIfEmptyHandler(handler);

    data_service::SetDataServiceIfEmptyHandler(handler);

    etcdserverpb::SetEtcdIfEmptyHandler(handler);

    friendpb::SetFriendIfEmptyHandler(handler);

    guildpb::SetGuildIfEmptyHandler(handler);

    guildpb::SetGuildInternalIfEmptyHandler(handler);

    loginpb::SetLoginIfEmptyHandler(handler);

    match::SetMatchInternalIfEmptyHandler(handler);

    match::SetMatchServiceIfEmptyHandler(handler);

    player_inbox::SetPlayerInboxIfEmptyHandler(handler);

    player_inbox::SetPlayerInboxAdminIfEmptyHandler(handler);

    scene_manager::SetSceneManagerServiceIfEmptyHandler(handler);

    scene_node::SetSceneNodeServiceIfEmptyHandler(handler);

    teampb::SetTeamIfEmptyHandler(handler);

    trade::SetJubaozhaiIfEmptyHandler(handler);

    trade::SetTradeAdminIfEmptyHandler(handler);

}

void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){

    ::SetBattleNodeHandler(handler);

    ::SetPlayerBattleHandler(handler);

    chatpb::SetChatHandler(handler);

    client_rpc_router::SetClientRpcRouterHandler(handler);

    data_service::SetDataServiceHandler(handler);

    etcdserverpb::SetEtcdHandler(handler);

    friendpb::SetFriendHandler(handler);

    guildpb::SetGuildHandler(handler);

    guildpb::SetGuildInternalHandler(handler);

    loginpb::SetLoginHandler(handler);

    match::SetMatchInternalHandler(handler);

    match::SetMatchServiceHandler(handler);

    player_inbox::SetPlayerInboxHandler(handler);

    player_inbox::SetPlayerInboxAdminHandler(handler);

    scene_manager::SetSceneManagerServiceHandler(handler);

    scene_node::SetSceneNodeServiceHandler(handler);

    teampb::SetTeamHandler(handler);

    trade::SetJubaozhaiHandler(handler);

    trade::SetTradeAdminHandler(handler);

}

void SetIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler){

    ::SetBattleNodeIfEmptyFailedHandler(handler);

    ::SetPlayerBattleIfEmptyFailedHandler(handler);

    chatpb::SetChatIfEmptyFailedHandler(handler);

    client_rpc_router::SetClientRpcRouterIfEmptyFailedHandler(handler);

    data_service::SetDataServiceIfEmptyFailedHandler(handler);

    etcdserverpb::SetEtcdIfEmptyFailedHandler(handler);

    friendpb::SetFriendIfEmptyFailedHandler(handler);

    guildpb::SetGuildIfEmptyFailedHandler(handler);

    guildpb::SetGuildInternalIfEmptyFailedHandler(handler);

    loginpb::SetLoginIfEmptyFailedHandler(handler);

    match::SetMatchInternalIfEmptyFailedHandler(handler);

    match::SetMatchServiceIfEmptyFailedHandler(handler);

    player_inbox::SetPlayerInboxIfEmptyFailedHandler(handler);

    player_inbox::SetPlayerInboxAdminIfEmptyFailedHandler(handler);

    scene_manager::SetSceneManagerServiceIfEmptyFailedHandler(handler);

    scene_node::SetSceneNodeServiceIfEmptyFailedHandler(handler);

    teampb::SetTeamIfEmptyFailedHandler(handler);

    trade::SetJubaozhaiIfEmptyFailedHandler(handler);

    trade::SetTradeAdminIfEmptyFailedHandler(handler);

}

void SetFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler){

    ::SetBattleNodeFailedHandler(handler);

    ::SetPlayerBattleFailedHandler(handler);

    chatpb::SetChatFailedHandler(handler);

    client_rpc_router::SetClientRpcRouterFailedHandler(handler);

    data_service::SetDataServiceFailedHandler(handler);

    etcdserverpb::SetEtcdFailedHandler(handler);

    friendpb::SetFriendFailedHandler(handler);

    guildpb::SetGuildFailedHandler(handler);

    guildpb::SetGuildInternalFailedHandler(handler);

    loginpb::SetLoginFailedHandler(handler);

    match::SetMatchInternalFailedHandler(handler);

    match::SetMatchServiceFailedHandler(handler);

    player_inbox::SetPlayerInboxFailedHandler(handler);

    player_inbox::SetPlayerInboxAdminFailedHandler(handler);

    scene_manager::SetSceneManagerServiceFailedHandler(handler);

    scene_node::SetSceneNodeServiceFailedHandler(handler);

    teampb::SetTeamFailedHandler(handler);

    trade::SetJubaozhaiFailedHandler(handler);

    trade::SetTradeAdminFailedHandler(handler);

}

void SetGrpcCallDeadline(uint32_t nodeType, std::chrono::milliseconds deadline){
    if (static_cast<uint32_t>(common::base::eNodeType::BattleNodeService) == nodeType) {
        ::SetBattleNodeCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::BattleNodeService) == nodeType) {
        ::SetPlayerBattleCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::ChatNodeService) == nodeType) {
        chatpb::SetChatCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::ClientRpcRouterNodeService) == nodeType) {
        client_rpc_router::SetClientRpcRouterCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::DataServiceNodeService) == nodeType) {
        data_service::SetDataServiceCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::EtcdNodeService) == nodeType) {
        etcdserverpb::SetEtcdCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::FriendNodeService) == nodeType) {
        friendpb::SetFriendCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::GuildNodeService) == nodeType) {
        guildpb::SetGuildCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::GuildNodeService) == nodeType) {
        guildpb::SetGuildInternalCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::LoginNodeService) == nodeType) {
        loginpb::SetLoginCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::MatchNodeService) == nodeType) {
        match::SetMatchInternalCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::MatchNodeService) == nodeType) {
        match::SetMatchServiceCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::PlayerInboxNodeService) == nodeType) {
        player_inbox::SetPlayerInboxCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::PlayerInboxNodeService) == nodeType) {
        player_inbox::SetPlayerInboxAdminCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::SceneManagerNodeService) == nodeType) {
        scene_manager::SetSceneManagerServiceCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::SceneManagerNodeService) == nodeType) {
        scene_node::SetSceneNodeServiceCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::TeamNodeService) == nodeType) {
        teampb::SetTeamCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::TradeNodeService) == nodeType) {
        trade::SetJubaozhaiCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::TradeNodeService) == nodeType) {
        trade::SetTradeAdminCallDeadline(deadline);
    }
}

void BeginShutdownGrpcNode(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity) || !registry.all_of<grpc::CompletionQueue>(entity)) return;
    auto* slot = registry.try_get<GrpcClientRuntimePtr>(entity);
    if (slot == nullptr || !*slot) {
        LOG_FATAL << "gRPC completion queue has no lifetime owner";
    }
    auto runtime = *slot;
    if (runtime->closing) return;
    runtime->closing = true;
    for (auto* context : runtime->calls) context->TryCancel();
}

// 每次轮询都重取组件；业务完成回调可能重入关闭或改变注册表。
// 关闭期间先消费取消后的 Read/Write，再发 Finish；全部调用结束后才 Shutdown CQ。
bool PollGrpcNodeShutdown(entt::registry& registry, entt::entity e) {
    if (!registry.valid(e)) return true;
    const auto nodeType = NodeUtils::GetRegistryType(registry);
    for (;;) {
        if (!registry.valid(e)) return true;
        auto* queue = registry.try_get<grpc::CompletionQueue>(e);
        if (queue == nullptr) return true;
        auto* slot = registry.try_get<GrpcClientRuntimePtr>(e);
        if (slot == nullptr || !*slot) {
            LOG_FATAL << "gRPC completion queue has no lifetime owner";
        }
        auto runtime = *slot;
        const bool callbacksIdle = runtime->queuedCompletions == 0 && runtime->activeCompletions == 0;
        if (runtime->drained) return runtime->calls.empty() && callbacksIdle;
        if (runtime->closing && runtime->calls.empty() && callbacksIdle && !runtime->queueShutdown) {
            runtime->queueShutdown = true;
            queue->Shutdown();
        }
        void* got_tag = nullptr;
        bool ok = false;
        gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
        const auto result = queue->AsyncNext(&got_tag, &ok, tm);
        if (result == grpc::CompletionQueue::SHUTDOWN) {
            runtime->drained = true;
            return runtime->calls.empty() && callbacksIdle;
        }
        if (result != grpc::CompletionQueue::GOT_EVENT) return false;
        auto* grpcTag = static_cast<GrpcTag*>(got_tag);
        const auto messageId = grpcTag->messageId;
        auto& completeQueueComp = *queue;
        const GrpcClientCompletionScope dispatchScope(runtime);
            if (common::base::eNodeType::BattleNodeService == nodeType &&
                (messageId == 146u || messageId == 147u || messageId == 159u || messageId == 160u || messageId == 178u)) {
                ::HandleBattleNodeCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::BattleNodeService == nodeType &&
                (messageId == 139u || messageId == 140u || messageId == 143u || messageId == 144u || messageId == 149u || messageId == 150u || messageId == 158u || messageId == 161u || messageId == 162u || messageId == 165u || messageId == 166u || messageId == 177u)) {
                ::HandlePlayerBattleCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::ChatNodeService == nodeType &&
                (messageId == 28u || messageId == 61u)) {
                chatpb::HandleChatCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::ClientRpcRouterNodeService == nodeType &&
                (messageId == 176u)) {
                client_rpc_router::HandleClientRpcRouterCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::DataServiceNodeService == nodeType &&
                (messageId == 86u || messageId == 87u || messageId == 88u || messageId == 89u || messageId == 90u || messageId == 91u || messageId == 92u || messageId == 93u || messageId == 96u || messageId == 97u || messageId == 98u || messageId == 99u || messageId == 100u || messageId == 101u || messageId == 105u || messageId == 108u || messageId == 114u || messageId == 129u || messageId == 180u || messageId == 229u || messageId == 231u || messageId == 237u || messageId == 248u || messageId == 258u)) {
                data_service::HandleDataServiceCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::EtcdNodeService == nodeType &&
                (messageId == 0u || messageId == 4u || messageId == 5u || messageId == 6u || messageId == 13u || messageId == 20u || messageId == 22u || messageId == 25u || messageId == 59u || messageId == 73u || messageId == 80u)) {
                etcdserverpb::HandleEtcdCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::FriendNodeService == nodeType &&
                (messageId == 2u || messageId == 7u || messageId == 11u || messageId == 12u || messageId == 119u || messageId == 230u || messageId == 232u || messageId == 234u || messageId == 235u || messageId == 236u || messageId == 238u)) {
                friendpb::HandleFriendCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::GuildNodeService == nodeType &&
                (messageId == 8u || messageId == 15u || messageId == 19u || messageId == 27u || messageId == 29u || messageId == 35u || messageId == 38u || messageId == 39u || messageId == 52u || messageId == 53u || messageId == 60u || messageId == 76u || messageId == 120u || messageId == 216u || messageId == 217u || messageId == 218u || messageId == 219u || messageId == 220u || messageId == 221u || messageId == 222u || messageId == 223u || messageId == 228u || messageId == 233u || messageId == 239u || messageId == 240u || messageId == 241u || messageId == 242u || messageId == 243u)) {
                guildpb::HandleGuildCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::GuildNodeService == nodeType &&
                (messageId == 244u)) {
                guildpb::HandleGuildInternalCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::LoginNodeService == nodeType &&
                (messageId == 14u || messageId == 17u || messageId == 26u || messageId == 48u || messageId == 58u || messageId == 111u || messageId == 118u || messageId == 127u || messageId == 138u)) {
                loginpb::HandleLoginCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::MatchNodeService == nodeType &&
                (messageId == 245u)) {
                match::HandleMatchInternalCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::MatchNodeService == nodeType &&
                (messageId == 148u || messageId == 151u || messageId == 152u || messageId == 153u || messageId == 154u || messageId == 156u || messageId == 157u || messageId == 163u || messageId == 164u || messageId == 179u)) {
                match::HandleMatchServiceCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::PlayerInboxNodeService == nodeType &&
                (messageId == 255u || messageId == 257u)) {
                player_inbox::HandlePlayerInboxCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::PlayerInboxNodeService == nodeType &&
                (messageId == 256u || messageId == 259u || messageId == 260u || messageId == 261u || messageId == 262u || messageId == 263u)) {
                player_inbox::HandlePlayerInboxAdminCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::SceneManagerNodeService == nodeType &&
                (messageId == 16u || messageId == 44u || messageId == 46u || messageId == 85u)) {
                scene_manager::HandleSceneManagerServiceCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::SceneManagerNodeService == nodeType &&
                (messageId == 122u || messageId == 123u || messageId == 128u || messageId == 142u || messageId == 145u || messageId == 224u || messageId == 225u || messageId == 227u)) {
                scene_node::HandleSceneNodeServiceCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::TeamNodeService == nodeType &&
                (messageId == 201u || messageId == 202u || messageId == 203u || messageId == 204u || messageId == 205u || messageId == 206u || messageId == 207u || messageId == 208u || messageId == 209u || messageId == 210u || messageId == 211u || messageId == 212u || messageId == 213u || messageId == 214u || messageId == 215u)) {
                teampb::HandleTeamCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::TradeNodeService == nodeType &&
                (messageId == 196u || messageId == 197u || messageId == 198u || messageId == 200u)) {
                trade::HandleJubaozhaiCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
            else if (common::base::eNodeType::TradeNodeService == nodeType &&
                (messageId == 199u)) {
                trade::HandleTradeAdminCompletedQueueMessage(registry, e, completeQueueComp, ok, grpcTag);
            }
    }
}

void HandleCompletedQueueMessage(entt::registry& registry) {
    const auto view = registry.view<grpc::CompletionQueue>();
    const std::vector<entt::entity> entities(view.begin(), view.end());
    for (const auto entity : entities) {
        (void)PollGrpcNodeShutdown(registry, entity);
    }
}

void InitGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity){
    auto nodeType = NodeUtils::GetRegistryType(registry);
    registry.emplace<GrpcClientRuntimePtr>(nodeEntity, std::make_shared<GrpcClientRuntime>());
    registry.emplace<grpc::CompletionQueue>(nodeEntity);
    if (common::base::eNodeType::BattleNodeService == nodeType) {
        ::InitBattleNodeGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::BattleNodeService == nodeType) {
        ::InitPlayerBattleGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::ChatNodeService == nodeType) {
        chatpb::InitChatGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::ClientRpcRouterNodeService == nodeType) {
        client_rpc_router::InitClientRpcRouterGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::DataServiceNodeService == nodeType) {
        data_service::InitDataServiceGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::EtcdNodeService == nodeType) {
        etcdserverpb::InitEtcdGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::FriendNodeService == nodeType) {
        friendpb::InitFriendGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::GuildNodeService == nodeType) {
        guildpb::InitGuildGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::GuildNodeService == nodeType) {
        guildpb::InitGuildInternalGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::LoginNodeService == nodeType) {
        loginpb::InitLoginGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::MatchNodeService == nodeType) {
        match::InitMatchInternalGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::MatchNodeService == nodeType) {
        match::InitMatchServiceGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::PlayerInboxNodeService == nodeType) {
        player_inbox::InitPlayerInboxGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::PlayerInboxNodeService == nodeType) {
        player_inbox::InitPlayerInboxAdminGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::SceneManagerNodeService == nodeType) {
        scene_manager::InitSceneManagerServiceGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::SceneManagerNodeService == nodeType) {
        scene_node::InitSceneNodeServiceGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::TeamNodeService == nodeType) {
        teampb::InitTeamGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::TradeNodeService == nodeType) {
        trade::InitJubaozhaiGrpcNode(channel, registry, nodeEntity);
    }
    if (common::base::eNodeType::TradeNodeService == nodeType) {
        trade::InitTradeAdminGrpcNode(channel, registry, nodeEntity);
    }
}
