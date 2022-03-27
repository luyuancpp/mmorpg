#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_SCENE_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_SCENE_H_
#include "player_service.h"
#include "logic_proto/player_scene.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace playerservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerSceneServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const playerservice::SeceneTestRequest* request,
        playerservice::SeceneTestResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const playerservice::SeceneTestRequest*>( request),
            ::google::protobuf::internal::DownCast<playerservice::SeceneTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace playerservice
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_SCENE_H_
