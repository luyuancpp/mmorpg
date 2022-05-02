#pragma once
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
    void EnterSceneBySceneIdC2S(EntityPtr& entity,
        const ::EnterSeceneBySceneIdC2SRequest* request,
        ::EnterSeceneBySceneIdC2SResponse* response);

    void EnterSceneBySceneConfigIdC2S(EntityPtr& entity,
        const ::EnterSeceneByConfigIdC2SRequest* request,
        ::EnterSeceneByConfigIdC2SResponse* response);

    void PushEnterSceneS2C(EntityPtr& entity,
        const ::EnterSeceneS2C* request,
        ::google::protobuf::Empty* response);
    void PushSceneInfoS2C(EntityPtr& entity,
        const ::SceneInfoS2C* request,
        ::google::protobuf::Empty* response);
        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterSceneBySceneIdC2S(entity,
            ::google::protobuf::internal::DownCast<const ::EnterSeceneBySceneIdC2SRequest*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneBySceneIdC2SResponse*>(response));
        break;
        case 1:
            EnterSceneBySceneConfigIdC2S(entity,
            ::google::protobuf::internal::DownCast<const ::EnterSeceneByConfigIdC2SRequest*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneByConfigIdC2SResponse*>(response));
        break;
        case 2:
            PushEnterSceneS2C(entity,
            ::google::protobuf::internal::DownCast<const ::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 3:
            PushSceneInfoS2C(entity,
            ::google::protobuf::internal::DownCast<const ::SceneInfoS2C*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
