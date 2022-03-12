#ifndef GAME_SERVER_SRC_SERVICE_C2GS_H_
#define GAME_SERVER_SRC_SERVICE_C2GS_H_
#include "c2gs_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace c2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerC2GsServiceImpl : public PlayerService {
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const c2gs::EnterSceneRequest* request,
        c2gs::EnterSceneResponse* response)override;

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::PROTOBUF_NAMESPACE_ID::Message* request,
        ::PROTOBUF_NAMESPACE_ID::Message* response)
        {
    switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const c2gs::EnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<const c2gs::returns*>( response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
};
}// namespace c2gs
#endif//GAME_SERVER_SRC_SERVICE_C2GS_H_
