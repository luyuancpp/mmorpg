#include "node_event_handler.h"
#include "proto/logic/event/node_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
#include "network/rpc_session.h"
///<<< END WRITING YOUR CODE


void NodeEventHandler::Register()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().connect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnSceneNodeRemovePbEvent>().connect<&NodeEventHandler::OnSceneNodeRemovePbEventHandler>();
    tls.dispatcher.sink<OnNodeAddPbEvent>().connect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnNodeRemovePbEvent>().connect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().disconnect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnSceneNodeRemovePbEvent>().disconnect<&NodeEventHandler::OnSceneNodeRemovePbEventHandler>();
    tls.dispatcher.sink<OnNodeAddPbEvent>().disconnect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnNodeRemovePbEvent>().disconnect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
}

void NodeEventHandler::OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
		// Log that we have received a RegisterGame request
	LOG_INFO << "Received RegisterGame request.";


	//// Search for a matching client connection and register the game node
	//for (const auto& [entity, session] : tls.networkRegistry.view<RpcSession>().each())
	//{
	//	if (session.connection->peerAddress().toIpPort() == clientAddr.toIpPort())
	//	{
	//		LOG_INFO << "Found matching client connection for registration.";
	//		clientFound = true;

	//		const auto newGameNode = tls.sceneNodeRegistry.create(gameNodeId);
	//		if (newGameNode == entt::null)
	//		{
	//			LOG_ERROR << "Failed to create game node " << request->scene_node_id();
	//			return;
	//		}

	//		// Create game node pointer and add components
	//		auto gameNodePtr = std::make_shared<RpcSessionPtr::element_type>(session.connection);
	//		AddMainSceneNodeComponent(tls.sceneNodeRegistry, newGameNode);
	//		tls.sceneNodeRegistry.emplace<RpcSessionPtr>(newGameNode, gameNodePtr);
	//		tls.sceneNodeRegistry.emplace<InetAddress>(newGameNode, serverAddr);
	//		LOG_INFO << "Game node " << request->scene_node_id() << " created and registered.";
	//		break;
	//	}
	//}

	//// If no matching client connection found, log a warning and return
	//if (!clientFound)
	//{
	//	LOG_WARN << "Client not found for registration. Ignoring request.";
	//	return;
	//}

	//LOG_INFO << "Game registered: " << MessageToJsonString(request);

	//// Update game node type based on server type
	//if (request->scene_node_type() == eGameNodeType::kMainSceneCrossNode)
	//{
	//	tls.sceneNodeRegistry.remove<MainSceneNode>(gameNodeId);
	//	tls.sceneNodeRegistry.emplace<CrossMainSceneNode>(gameNodeId);
	//	LOG_INFO << "Game node " << request->scene_node_id() << " updated to CrossMainSceneNode.";
	//}
	//else if (request->scene_node_type() == eGameNodeType::kRoomNode)
	//{
	//	tls.sceneNodeRegistry.remove<MainSceneNode>(gameNodeId);
	//	tls.sceneNodeRegistry.emplace<RoomSceneNode>(gameNodeId);
	//	LOG_INFO << "Game node " << request->scene_node_id() << " updated to RoomSceneNode.";
	//}
	//else if (request->scene_node_type() == eGameNodeType::kRoomSceneCrossNode)
	//{
	//	tls.sceneNodeRegistry.remove<MainSceneNode>(gameNodeId);
	//	tls.sceneNodeRegistry.emplace<CrossRoomSceneNode>(gameNodeId);
	//	LOG_INFO << "Game node " << request->scene_node_id() << " updated to CrossRoomSceneNode.";
	//}

///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnSceneNodeRemovePbEventHandler(const OnSceneNodeRemovePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnNodeAddPbEventHandler(const OnNodeAddPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
