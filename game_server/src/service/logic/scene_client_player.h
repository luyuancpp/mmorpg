#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_CLIENT_PLAYER_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_CLIENT_PLAYER_H_
#include "../player_service.h"
#include "logic_proto/scene_client_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace clientplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const clientplayer::SeceneTestRequest* request,
        clientplayer::SeceneTestResponse* response);

    void EnterSceneNotify(common::EntityPtr& entity,
        const clientplayer::EnterSeceneS2C* request,
        clientplayer::EnterSeceneS2C* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const clientplayer::SeceneTestRequest*>( request),
            ::google::protobuf::internal::DownCast<clientplayer::SeceneTestResponse*>(response));
        break;
        case 1:
            EnterSceneNotify(entity,
            ::google::protobuf::internal::DownCast<const clientplayer::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<clientplayer::EnterSeceneS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace clientplayer
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_CLIENT_PLAYER_H_
