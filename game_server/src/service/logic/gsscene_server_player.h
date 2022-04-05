#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_SERVER_PLAYER_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_SERVER_PLAYER_H_
#include "player_service.h"
#include "logic_proto/scene_server_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace serverplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void LogincMs2Gs(common::EntityPtr& entity,
        const serverplayer::Ms2GsLoginRequest* request,
        ::google::protobuf::Empty* response);
    void ReconnectMs2Gs(common::EntityPtr& entity,
        const serverplayer::Ms2GsReconnectRequest* request,
        ::google::protobuf::Empty* response);
        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            LogincMs2Gs(entity,
            ::google::protobuf::internal::DownCast<const serverplayer::Ms2GsLoginRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            ReconnectMs2Gs(entity,
            ::google::protobuf::internal::DownCast<const serverplayer::Ms2GsReconnectRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace serverplayer
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_SERVER_PLAYER_H_
