#pragma once
#include "player_service.h"
#include "logic_proto/scene_server_player.pb.h"
class ServerPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void EnterSceneGs2Controller(entt::entity player,
        const ::Gs2ControllerEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    void EnterSceneController2Gs(entt::entity player,
        const ::Controller2GsEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    void LeaveSceneGs2Controller(entt::entity player,
        const ::Gs2ControllerLeaveSceneRequest* request,
        ::google::protobuf::Empty* response);
    void LeaveSceneController2Gs(entt::entity player,
        const ::Controller2GsLeaveSceneRequest* request,
        ::google::protobuf::Empty* response);
    void Gs2ControllerLeaveSceneAsyncSavePlayerComplete(entt::entity player,
        const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest* request,
        ::google::protobuf::Empty* response);
    void Send2ClientEnterScene(entt::entity player,
        const ::Send2ClientEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            EnterSceneGs2Controller(player,
            ::google::protobuf::internal::DownCast<const ::Gs2ControllerEnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            EnterSceneController2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Controller2GsEnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 2:
            LeaveSceneGs2Controller(player,
            ::google::protobuf::internal::DownCast<const ::Gs2ControllerLeaveSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 3:
            LeaveSceneController2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Controller2GsLeaveSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 4:
            Gs2ControllerLeaveSceneAsyncSavePlayerComplete(player,
            ::google::protobuf::internal::DownCast<const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 5:
            Send2ClientEnterScene(player,
            ::google::protobuf::internal::DownCast<const ::Send2ClientEnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
