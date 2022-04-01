#ifndef _LOGIC_PROTO_PLAYER_SCENE_MS_H_
#define _LOGIC_PROTO_PLAYER_SCENE_MS_H_
#include "player_service.h"
#include "logic_proto/player/scene_ms.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace msplayerservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerSceneServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const msplayerservice::SeceneTestRequest* request,
        msplayerservice::SeceneTestResponse* response);

    void EnterSceneNotify(common::EntityPtr& entity,
        const msplayerservice::EnterSeceneS2C* request,
        msplayerservice::EnterSeceneS2C* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const msplayerservice::SeceneTestRequest*>( request),
            ::google::protobuf::internal::DownCast<msplayerservice::SeceneTestResponse*>(response));
        break;
        case 1:
            EnterSceneNotify(entity,
            ::google::protobuf::internal::DownCast<const msplayerservice::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<msplayerservice::EnterSeceneS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace msplayerservice
#endif//_LOGIC_PROTO_PLAYER_SCENE_MS_H_
