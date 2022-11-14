#pragma once
#include "player_service.h"
#include "logic_proto/common_client_player.pb.h"
class ClientPlayerCommonServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void PushTipsS2C(entt::entity player,
        const ::TipsS2C* request,
        ::TipsS2C* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            PushTipsS2C(player,
            ::google::protobuf::internal::DownCast<const ::TipsS2C*>( request),
            ::google::protobuf::internal::DownCast<::TipsS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
