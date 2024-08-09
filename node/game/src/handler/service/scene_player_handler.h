#pragma once
#include "logic/client_player/scene_client_player.pb.h"
#include "player_service.h"
class ClientPlayerSceneServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void EnterScene(entt::entity player,
		const ::EnterSceneC2SRequest* request,
		::EnterSceneC2SResponse* response);

	static void NotifyEnterScene(entt::entity player,
		const ::EnterSceneS2C* request,
		::Empty* response);

	static void SceneInfoC2S(entt::entity player,
		const ::SceneInfoRequest* request,
		::Empty* response);

	static void NotifySceneInfo(entt::entity player,
		const ::SceneInfoS2C* request,
		::Empty* response);

	static void NotifyActorCreate(entt::entity player,
		const ::ActorCreateS2C* request,
		::Empty* response);

	static void NotifyActorDestroy(entt::entity player,
		const ::ActorDestroyS2C* request,
		::Empty* response);

	static void NotifyActorListCreate(entt::entity player,
		const ::ActorListCreateS2C* request,
		::Empty* response);

	static void NotifyActorListDestroy(entt::entity player,
		const ::ActorListDestroyS2C* request,
		::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			EnterScene(player,
			::google::protobuf::internal::DownCast<const EnterSceneC2SRequest*>(request),
			::google::protobuf::internal::DownCast<EnterSceneC2SResponse*>(response));
		break;
		case 1:
			NotifyEnterScene(player,
			::google::protobuf::internal::DownCast<const EnterSceneS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 2:
			SceneInfoC2S(player,
			::google::protobuf::internal::DownCast<const SceneInfoRequest*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 3:
			NotifySceneInfo(player,
			::google::protobuf::internal::DownCast<const SceneInfoS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 4:
			NotifyActorCreate(player,
			::google::protobuf::internal::DownCast<const ActorCreateS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 5:
			NotifyActorDestroy(player,
			::google::protobuf::internal::DownCast<const ActorDestroyS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 6:
			NotifyActorListCreate(player,
			::google::protobuf::internal::DownCast<const ActorListCreateS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 7:
			NotifyActorListDestroy(player,
			::google::protobuf::internal::DownCast<const ActorListDestroyS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
