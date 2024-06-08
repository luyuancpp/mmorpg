#pragma once
#include "client_player_proto/scene_client_player.pb.h"
#include "player_service.h"
class ClientPlayerSceneServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void EnterSceneC2S(entt::entity player,
		const ::EnterSceneC2SRequest* request,
		::EnterSceneC2SResponse* response);

	static void PushEnterSceneS2C(entt::entity player,
		const ::EnterSceneS2C* request,
		::EnterSceneS2C* response);

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
			::google::protobuf::internal::DownCast<const EnterSceneC2SRequest*>( request),
			::google::protobuf::internal::DownCast<EnterSceneC2SResponse*>(response));
		break;
		case 1:
			PushEnterSceneS2C(player,
			::google::protobuf::internal::DownCast<const EnterSceneS2C*>( request),
			::google::protobuf::internal::DownCast<EnterSceneS2C*>(response));
		break;
		case 2:
			PushSceneInfoS2C(player,
			::google::protobuf::internal::DownCast<const SceneInfoS2C*>( request),
			::google::protobuf::internal::DownCast<SceneInfoS2C*>(response));
		break;
		default:
		break;
		}
	}

};