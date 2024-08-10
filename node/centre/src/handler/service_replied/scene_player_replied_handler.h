#pragma once
#include "logic/client_player/scene_player.pb.h"
#include "player_service_replied.h"

class ClientPlayerSceneServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
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
			nullptr,
			::google::protobuf::internal::DownCast<EnterSceneC2SResponse*>(response));
		break;
		case 1:
			NotifyEnterScene(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 2:
			SceneInfoC2S(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 3:
			NotifySceneInfo(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 4:
			NotifyActorCreate(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 5:
			NotifyActorDestroy(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 6:
			NotifyActorListCreate(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 7:
			NotifyActorListDestroy(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
