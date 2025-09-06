
#include "centre_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "proto/common/node.pb.h"
#include "threading/node_context_manager.h"
#include <threading/registry_manager.h>
#include <modules/scene/system/room_common.h>
#include "modules/scene/system/room_param.h"
///<<< END WRITING YOUR CODE


void CentreSceneHandler::RegisterScene(::google::protobuf::RpcController* controller, const ::RegisterSceneRequest* request,
	::RegisterSceneResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto&& roomInfo : request->scenes_info())
	{
		RoomCommon::CreateRoomOnRoomNode(
			{ .node = entt::entity{request->scene_node_id()}, .roomInfo = roomInfo });

		LOG_INFO << "Scene " << roomInfo.DebugString()
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
	const entt::entity room{ request->scene() };
	if (!tlsRegistryManager.roomRegistry.valid(room))
	{
		LOG_ERROR << "Room not found: " << request->scene();
		return;
	}

	const entt::entity gameNode{ request->scene_node_id() };
	if (!tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(gameNode))
	{
		LOG_ERROR << "Node not found: " << request->scene_node_id();
		return;
	}

	RoomCommon::DestroyRoom({ gameNode, room });
	///<<< END WRITING YOUR CODE
}


