#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_SCENE_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_SCENE_H_
#include "player_service.h"
#include "logic_proto/player_scene.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace playerscene{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerC2GsSceneServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const playerscene::EnterSceneRequest* request,
        playerscene::EnterSceneResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const playerscene::EnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<playerscene::EnterSceneResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace playerscene
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_SCENE_H_
