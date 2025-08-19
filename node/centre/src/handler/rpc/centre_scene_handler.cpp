
#include "centre_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "scene/system/scene_system.h"
#include "proto/common/node.pb.h"
#include "thread_local/node_context_manager.h"
///<<< END WRITING YOUR CODE


void CentreSceneHandler::RegisterScene(::google::protobuf::RpcController* controller, const ::RegisterSceneRequest* request,
	::RegisterSceneResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto&& sceneInfo : request->scenes_info())
	{
		SceneUtil::CreateSceneToSceneNode(
			{ .node = entt::entity{request->scene_node_id()}, .sceneInfo = sceneInfo });

		LOG_INFO << "Scene " << sceneInfo.DebugString()
			<< " successfully created and attached to SceneNode ["
			<< request->scene_node_id() << "].";
	}

	
	///<<< END WRITING YOUR CODE
}



void CentreSceneHandler::UnRegisterScene(::google::protobuf::RpcController* controller, const ::UnRegisterSceneRequest* request,
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

	const entt::entity gameNode{ request->scene_node_id() };
	if (!NodeContextManager::Instance().GetRegistry(eNodeType::SceneNodeService).valid(gameNode))
	{
		LOG_ERROR << "Node not found: " << request->scene_node_id();
		return;
	}

	SceneUtil::DestroyScene({ gameNode, scene });
	///<<< END WRITING YOUR CODE
}


