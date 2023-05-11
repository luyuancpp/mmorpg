#pragma once
#include "server_player_proto/controller_scene_server_player.pb.h"
#include "player_service.h"
class ControllerSceneServerPlayerServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void EnterScene(entt::entity player,
		const ::ControllerEnterSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveScene(entt::entity player,
		const ::ControllerLeaveSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveSceneAsyncSavePlayerComplete(entt::entity player,
		const ::ControllerLeaveSceneAsyncSavePlayerCompleteRequest* request,
		::google::protobuf::Empty* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			EnterScene(player,
			::google::protobuf::internal::DownCast<const ControllerEnterSceneRequest*>( request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			LeaveScene(player,
			::google::protobuf::internal::DownCast<const ControllerLeaveSceneRequest*>( request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 2:
			LeaveSceneAsyncSavePlayerComplete(player,
			::google::protobuf::internal::DownCast<const ControllerLeaveSceneAsyncSavePlayerCompleteRequest*>( request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
