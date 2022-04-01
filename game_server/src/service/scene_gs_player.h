#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_SCENE_GS_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_SCENE_GS_H_
#include "player_service.h"
#include "logic_proto/scene_gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace gsplayerservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerSceneServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const gsplayerservice::SeceneTestRequest* request,
        gsplayerservice::SeceneTestResponse* response);

    void EnterSceneNotify(common::EntityPtr& entity,
        const gsplayerservice::EnterSeceneS2C* request,
        gsplayerservice::EnterSeceneS2C* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const gsplayerservice::SeceneTestRequest*>( request),
            ::google::protobuf::internal::DownCast<gsplayerservice::SeceneTestResponse*>(response));
        break;
        case 1:
            EnterSceneNotify(entity,
            ::google::protobuf::internal::DownCast<const gsplayerservice::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<gsplayerservice::EnterSeceneS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace gsplayerservice
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_SCENE_GS_H_
