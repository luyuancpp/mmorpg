#ifndef _LOGIC_PROTO_PLAYER_SCENE_GS_H_
#define _LOGIC_PROTO_PLAYER_SCENE_GS_H_
#include "player_service.h"
#include "logic_proto/player/scene_gs.pb.h"
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

    void EnterSceneNotify(common::EntityPtr& entity,
        const playerservice::EnterSeceneS2C* request,
        playerservice::EnterSeceneS2C* response);

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
        case 1:
            EnterSceneNotify(entity,
            ::google::protobuf::internal::DownCast<const playerservice::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<playerservice::EnterSeceneS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace playerservice
#endif//_LOGIC_PROTO_PLAYER_SCENE_GS_H_
