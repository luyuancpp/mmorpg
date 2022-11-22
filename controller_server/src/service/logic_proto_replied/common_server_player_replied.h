#pragma once
#include "player_service_replied.h"
#include "logic_proto/common_server_player.pb.h"
class ServerPlayerLoginServiceRepliedImpl : public PlayerServiceReplied {
public:
    using PlayerService::PlayerService;
public:
    void UpdateSessionController2Gs(entt::entity player,
        ::google::protobuf::Empty* response);
    void Controller2GsLogin(entt::entity player,
        ::google::protobuf::Empty* response);
    void Controller2GsEnterGateSucceed(entt::entity player,
        ::google::protobuf::Empty* response);
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            UpdateSessionController2Gs(player,
::UpdateSessionController2GsRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            Controller2GsLogin(player,
::Controller2GsLoginRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 2:
            Controller2GsEnterGateSucceed(player,
::Controller2GsEnterGateSucceedRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
