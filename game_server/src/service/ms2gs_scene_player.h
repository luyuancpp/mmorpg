#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_MS2GS_SCENE_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_MS2GS_SCENE_H_
#include "player_service.h"
#include "logic_proto/ms2gs_scene.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace ms2gsscene{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerC2GsServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void OnLogin(common::EntityPtr& entity,
        const ms2gsscene::OnLoginRequest* request,
        ::google::protobuf::Empty* response);
        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            OnLogin(entity,
            ::google::protobuf::internal::DownCast<const ms2gsscene::OnLoginRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace ms2gsscene
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_MS2GS_SCENE_H_
