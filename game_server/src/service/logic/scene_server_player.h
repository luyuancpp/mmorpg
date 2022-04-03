#ifndef MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_SERVER_PLAYER_H_
#define MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_SERVER_PLAYER_H_
#include "../player_service.h"
#include "logic_proto/scene_server_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace serverplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerSceneServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterSceneGs2Ms(common::EntityPtr& entity,
        const serverplayer::SeceneGsRequest* request,
        serverplayer::SeceneGsResponse* response);

    void EnterSceneMs2Gs(common::EntityPtr& entity,
        const serverplayer::SeceneGsRequest* request,
        serverplayer::SeceneGsResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterSceneGs2Ms(entity,
            ::google::protobuf::internal::DownCast<const serverplayer::SeceneGsRequest*>( request),
            ::google::protobuf::internal::DownCast<serverplayer::SeceneGsResponse*>(response));
        break;
        case 1:
            EnterSceneMs2Gs(entity,
            ::google::protobuf::internal::DownCast<const serverplayer::SeceneGsRequest*>( request),
            ::google::protobuf::internal::DownCast<serverplayer::SeceneGsResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace serverplayer
#endif//MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_SERVER_PLAYER_H_
