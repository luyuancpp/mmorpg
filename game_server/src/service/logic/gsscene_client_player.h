#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_CLIENT_PLAYER_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_CLIENT_PLAYER_H_
#include "player_service.h"
#include "logic_proto/scene_client_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class ClientPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(EntityPtr& entity,
        const ::EnterSeceneC2SRequest* request,
        ::EnterSeceneC2SResponse* response);

    void EnterSceneNotify(EntityPtr& entity,
        const ::EnterSeceneS2C* request,
        ::EnterSeceneS2C* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const ::EnterSeceneC2SRequest*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneC2SResponse*>(response));
        break;
        case 1:
            EnterSceneNotify(entity,
            ::google::protobuf::internal::DownCast<const ::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_CLIENT_PLAYER_H_
