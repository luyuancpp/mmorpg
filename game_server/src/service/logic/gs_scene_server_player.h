#pragma once
#include "player_service.h"
#include "logic_proto/scene_server_player.pb.h"
class ServerPlayerSceneServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void EnterSceneGs2Ms(entt::entity player,
        const ::Gs2MsEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    void EnterSceneMs2Gs(entt::entity player,
        const ::Ms2GsEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    void LeaveSceneGs2Ms(entt::entity player,
        const ::Gs2MsLeaveSceneRequest* request,
        ::google::protobuf::Empty* response);
    void LeaveSceneMs2Gs(entt::entity player,
        const ::Ms2GsLeaveSceneRequest* request,
        ::google::protobuf::Empty* response);
    void Gs2ControllerLeaveSceneAsyncSavePlayerComplete(entt::entity player,
        const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest* request,
        ::google::protobuf::Empty* response);
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            EnterSceneGs2Ms(player,
            ::google::protobuf::internal::DownCast<const ::Gs2MsEnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            EnterSceneMs2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Ms2GsEnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 2:
            LeaveSceneGs2Ms(player,
            ::google::protobuf::internal::DownCast<const ::Gs2MsLeaveSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 3:
            LeaveSceneMs2Gs(player,
            ::google::protobuf::internal::DownCast<const ::Ms2GsLeaveSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 4:
            Gs2ControllerLeaveSceneAsyncSavePlayerComplete(player,
            ::google::protobuf::internal::DownCast<const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
