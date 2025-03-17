#pragma once
#include "proto/logic/server_player/centre_player_scene.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class CentrePlayerSceneServiceHandler : public ::PlayerService
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
			static_cast<const CentreEnterSceneRequest*>(request),
			static_cast<google::protobuf::Empty*>(response));
		break;
		case 1:
			LeaveScene(player,
			static_cast<const CentreLeaveSceneRequest*>(request),
			static_cast<google::protobuf::Empty*>(response));
		break;
		case 2:
			LeaveSceneAsyncSavePlayerComplete(player,
			static_cast<const CentreLeaveSceneAsyncSavePlayerCompleteRequest*>(request),
			static_cast<google::protobuf::Empty*>(response));
		break;
		case 3:
			SceneInfoC2S(player,
			static_cast<const SceneInfoRequest*>(request),
			static_cast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
