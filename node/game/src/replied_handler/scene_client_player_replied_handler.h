#pragma once
#include "client_player_proto/scene_client_player.pb.h"
#include "player_service_replied.h"
class ClientPlayerSceneServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void EnterSceneC2S(entt::entity player,
		const ::EnterSceneC2SRequest* request,
		::EnterSceneC2SResponse* response);

	static void PushEnterSceneS2C(entt::entity player,
		const ::EnterSceneS2C* request,
		::Empty* response);

	static void SceneInfoC2S(entt::entity player,
		const ::SceneInfoRequest* request,
		::SceneInfoResponse* response);

	static void PushSceneInfoS2C(entt::entity player,
		const ::SceneInfoS2C* request,
		::Empty* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			EnterSceneC2S(player,
			nullptr,
			::google::protobuf::internal::DownCast<EnterSceneC2SResponse*>(response));
		break;
		case 1:
			PushEnterSceneS2C(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 2:
			SceneInfoC2S(player,
			nullptr,
			::google::protobuf::internal::DownCast<SceneInfoResponse*>(response));
		break;
		case 3:
			PushSceneInfoS2C(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
