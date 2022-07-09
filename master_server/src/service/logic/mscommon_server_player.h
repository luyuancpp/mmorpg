#pragma once
#include "player_service.h"
#include "logic_proto/common_server_player.pb.h"
class ServerPlayerLoginServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void UpdateGateSessionMs2Gs(entt::entity player,
        const ::UpdateGateSessionGsRequest* request,
        ::google::protobuf::Empty* response);
    void LoginMs2Gs(entt::entity player,
        const ::Ms2GsLoginRequest* request,
        ::google::protobuf::Empty* response);
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            UpdateGateSessionMs2Gs(player,
            ::google::protobuf::internal::DownCast<const ::UpdateGateSessionGsRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            LoginMs2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Ms2GsLoginRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
