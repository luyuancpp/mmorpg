#pragma once
#include "client_player_proto/scene_client_player.pb.h"
#include "player_service_replied.h"
class ClientPlayerSceneServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void EnterSceneC2S(entt::entity player,
		const ::EnterSeceneC2SRequest* request,
		::EnterSeceneC2SResponse* response);

	static void PushEnterSceneS2C(entt::entity player,
		const ::EnterSeceneS2C* request,
		::EnterSeceneS2C* response);

	static void PushSceneInfoS2C(entt::entity player,
		const ::SceneInfoS2C* request,
		::SceneInfoS2C* response);

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
			::google::protobuf::internal::DownCast<EnterSeceneC2SResponse*>(response));
		break;
		case 1:
			PushEnterSceneS2C(player,
			nullptr,
			::google::protobuf::internal::DownCast<EnterSeceneS2C*>(response));
		break;
		case 2:
			PushSceneInfoS2C(player,
			nullptr,
			::google::protobuf::internal::DownCast<SceneInfoS2C*>(response));
		break;
		default:
		break;
		}
	}

};