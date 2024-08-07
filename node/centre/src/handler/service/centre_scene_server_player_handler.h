#pragma once
#include "logic/server_player/centre_scene_server_player.pb.h"
#include "player_service.h"
class CentreScenePlayerServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void EnterScene(entt::entity player,
		const ::CentreEnterSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveScene(entt::entity player,
		const ::CentreLeaveSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveSceneAsyncSavePlayerComplete(entt::entity player,
		const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
		::google::protobuf::Empty* response);

	static void SceneInfoC2S(entt::entity player,
		const ::SceneInfoRequest* request,
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
			::google::protobuf::internal::DownCast<const CentreEnterSceneRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			LeaveScene(player,
			::google::protobuf::internal::DownCast<const CentreLeaveSceneRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 2:
			LeaveSceneAsyncSavePlayerComplete(player,
			::google::protobuf::internal::DownCast<const CentreLeaveSceneAsyncSavePlayerCompleteRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 3:
			SceneInfoC2S(player,
			::google::protobuf::internal::DownCast<const SceneInfoRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
