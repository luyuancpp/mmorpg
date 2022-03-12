#ifndef GAME_SERVER_SRC_SERVICE_C2GS_H_
#define GAME_SERVER_SRC_SERVICE_C2GS_H_
#include "player_service.h"
#include "c2gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace c2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerC2GsServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const c2gs::EnterSceneRequest* request,
        c2gs::EnterSceneResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const c2gs::EnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<c2gs::EnterSceneResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace c2gs
#endif//GAME_SERVER_SRC_SERVICE_C2GS_H_
