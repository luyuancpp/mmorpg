#pragma once
#include "player_service.h"
#include "logic_proto/scene_server_player.pb.h"
class ServerPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void LoginMs2Gs(entt::entity& player,
        const ::Ms2GsLoginRequest* request,
        ::google::protobuf::Empty* response);
    void ReconnectMs2Gs(entt::entity& player,
        const ::Ms2GsReconnectRequest* request,
        ::google::protobuf::Empty* response);
    void EnterSceneGs2Ms(entt::entity& player,
        const ::Gs2MsEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity& player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            LoginMs2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Ms2GsLoginRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            ReconnectMs2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Ms2GsReconnectRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 2:
            EnterSceneGs2Ms(player,
            ::google::protobuf::internal::DownCast<const ::Gs2MsEnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
