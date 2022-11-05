#pragma once
#include "player_service.h"
#include "logic_proto/scene_client_player.pb.h"
class ClientPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void EnterSceneC2S(entt::entity player,
        const ::EnterSeceneC2SRequest* request,
        ::EnterSeceneC2SResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            EnterSceneC2S(player,
            ::google::protobuf::internal::DownCast<const ::EnterSeceneC2SRequest*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneC2SResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
