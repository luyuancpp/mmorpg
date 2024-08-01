#include "centre_scene_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "scene/util/scene_util.h"
///<<< END WRITING YOUR CODE
void CentreSceneServiceHandler::RegisterScene(::google::protobuf::RpcController* controller,
	const ::RegisterSceneRequest* request,
	::RegisterSceneResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto&& sceneInfo : request->scenes_info())
	{
		SceneUtil::CreateScene2GameNode(
			{ .node = entt::entity{request->game_node_id()}, .sceneInfo = sceneInfo });
	}
	///<<< END WRITING YOUR CODE
}

void CentreSceneServiceHandler::UnRegisterScene(::google::protobuf::RpcController* controller,
	const ::UnRegisterSceneRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const entt::entity scene{ request->scene() };
	if (!tls.sceneRegistry.valid(scene))
	{
		LOG_ERROR << "Scene not found: " << request->scene();
		return;
	}

	const entt::entity gameNode{ request->game_node_id() };
	if (!tls.gameNodeRegistry.valid(gameNode))
	{
		LOG_ERROR << "Node not found: " << request->game_node_id();
		return;
	}

	SceneUtil::DestroyScene({ gameNode, scene });
	///<<< END WRITING YOUR CODE
}

