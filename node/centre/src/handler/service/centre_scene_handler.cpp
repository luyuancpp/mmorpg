#include "centre_scene_handler.h"
#include "thread_local/storage.h"
#include "system/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "system/scene/scene_system.h"
///<<< END WRITING YOUR CODE
void CentreSceneServiceHandler::RegisterScene(::google::protobuf::RpcController* controller,
	const ::RegisterSceneRequest* request,
	::RegisterSceneResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	for (auto&& scene_info : request->scenes_info())
	{
		ScenesSystem::CreateScene2GameNode(
			{ .node = entt::entity{request->game_node_id()}, .scene_info = scene_info });
	}
///<<< END WRITING YOUR CODE
}

void CentreSceneServiceHandler::UnRegisterScene(::google::protobuf::RpcController* controller,
	const ::UnRegisterSceneRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const entt::entity scene{request->scene()};
	if (tls.scene_registry.valid(scene))
	{
		LOG_ERROR << "scene not found" << request->scene();
		return;
	}
	const entt::entity game_node{request->game_node_id()};
	if (tls.game_node_registry.valid(game_node))
	{
		LOG_ERROR << "node not found" << request->game_node_id();
		return;
	}
	ScenesSystem::DestroyScene({game_node, scene});
///<<< END WRITING YOUR CODE
}

