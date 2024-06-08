#pragma once
#include "server_player_proto/centre_scene_server_player.pb.h"
#include "player_service_replied.h"
class CentreScenePlayerServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void EnterScene(entt::entity player,
		const ::CentreEnterSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveScene(entt::entity player,
		const ::CentreLeaveSceneRequest* request,
		::google::protobuf::Empty* response);

	static void LeaveSceneAsyncSavePlayerComplete(entt::entity player,
		const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
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
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			LeaveScene(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 2:
			LeaveSceneAsyncSavePlayerComplete(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
