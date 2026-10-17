#include "grpc_init_client.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <google/protobuf/message.h>
#include <proto/common/base/node.pb.h>
#include <proto/common/base/common.pb.h>

    void SetBattleNodeHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetBattleNodeIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetBattleNodeFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetBattleNodeIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetBattleNodeCallDeadline(std::chrono::milliseconds deadline);

    void SetPlayerBattleHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerBattleIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerBattleFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerBattleIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerBattleCallDeadline(std::chrono::milliseconds deadline);

namespace chatpb {
    void SetChatHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetChatIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetChatFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetChatIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetChatCallDeadline(std::chrono::milliseconds deadline);
}

namespace client_rpc_router {
    void SetClientRpcRouterHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetClientRpcRouterIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetClientRpcRouterFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetClientRpcRouterIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetClientRpcRouterCallDeadline(std::chrono::milliseconds deadline);
}

namespace data_service {
    void SetDataServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetDataServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetDataServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetDataServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetDataServiceCallDeadline(std::chrono::milliseconds deadline);
}

namespace etcdserverpb {
    void SetEtcdHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetEtcdFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetEtcdIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetEtcdCallDeadline(std::chrono::milliseconds deadline);
}

namespace friendpb {
    void SetFriendHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetFriendIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetFriendFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetFriendIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetFriendCallDeadline(std::chrono::milliseconds deadline);
}

namespace guildpb {
    void SetGuildHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildCallDeadline(std::chrono::milliseconds deadline);
}

namespace guildpb {
    void SetGuildInternalHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildInternalIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetGuildInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetGuildInternalCallDeadline(std::chrono::milliseconds deadline);
}

namespace loginpb {
    void SetLoginHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetLoginFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetLoginIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetLoginCallDeadline(std::chrono::milliseconds deadline);
}

namespace mailpb {
    void SetMailHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMailIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMailFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMailIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMailCallDeadline(std::chrono::milliseconds deadline);
}

namespace mailpb {
    void SetMailAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMailAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMailAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMailAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMailAdminCallDeadline(std::chrono::milliseconds deadline);
}

namespace match {
    void SetMatchInternalHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchInternalIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchInternalCallDeadline(std::chrono::milliseconds deadline);
}

namespace match {
    void SetMatchServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetMatchServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetMatchServiceCallDeadline(std::chrono::milliseconds deadline);
}

namespace player_inbox {
    void SetPlayerInboxHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxCallDeadline(std::chrono::milliseconds deadline);
}

namespace player_inbox {
    void SetPlayerInboxAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetPlayerInboxAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetPlayerInboxAdminCallDeadline(std::chrono::milliseconds deadline);
}

namespace scene_manager {
    void SetSceneManagerServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneManagerServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneManagerServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneManagerServiceCallDeadline(std::chrono::milliseconds deadline);
}

namespace scene_node {
    void SetSceneNodeServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneNodeServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetSceneNodeServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneNodeServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetSceneNodeServiceCallDeadline(std::chrono::milliseconds deadline);
}

namespace teampb {
    void SetTeamHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTeamIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTeamFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTeamIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTeamCallDeadline(std::chrono::milliseconds deadline);
}

namespace trade {
    void SetJubaozhaiHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetJubaozhaiIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetJubaozhaiIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline);
}

namespace trade {
    void SetTradeAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTradeAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
    void SetTradeAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTradeAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
    void SetTradeAdminCallDeadline(std::chrono::milliseconds deadline);
}

void SetIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler){

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

    mailpb::SetMailIfEmptyHandler(handler);

    mailpb::SetMailAdminIfEmptyHandler(handler);

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

void SetHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler){

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

    mailpb::SetMailHandler(handler);

    mailpb::SetMailAdminHandler(handler);

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

    mailpb::SetMailIfEmptyFailedHandler(handler);

    mailpb::SetMailAdminIfEmptyFailedHandler(handler);

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

    mailpb::SetMailFailedHandler(handler);

    mailpb::SetMailAdminFailedHandler(handler);

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
    if (static_cast<uint32_t>(common::base::eNodeType::MailNodeService) == nodeType) {
        mailpb::SetMailCallDeadline(deadline);
    }
    if (static_cast<uint32_t>(common::base::eNodeType::MailNodeService) == nodeType) {
        mailpb::SetMailAdminCallDeadline(deadline);
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
