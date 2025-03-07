#pragma once
#include "proto/logic/server_player/game_player_scene.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class GamePlayerSceneServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void EnterScene(entt::entity player,
		const ::GsEnterSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveScene(entt::entity player,
		const ::GsLeaveSceneRequest* request,
		::google::protobuf::Empty* response);

	static void EnterSceneS2C(entt::entity player,
		const ::EnterSceneS2CRequest* request,
		::EnterScenerS2CResponse* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			EnterScene(player,
			::google::protobuf::internal::DownCast<const GsEnterSceneRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			LeaveScene(player,
			::google::protobuf::internal::DownCast<const GsLeaveSceneRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 2:
			EnterSceneS2C(player,
			::google::protobuf::internal::DownCast<const EnterSceneS2CRequest*>(request),
			::google::protobuf::internal::DownCast<EnterScenerS2CResponse*>(response));
			TRANSFER_ERROR_MESSAGE(::google::protobuf::internal::DownCast<EnterScenerS2CResponse*>(response));
		break;
		default:
		break;
		}
	}

};
