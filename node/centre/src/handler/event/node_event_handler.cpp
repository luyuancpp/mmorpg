#include "node_event_handler.h"
#include "proto/logic/event/node_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void NodeEventHandler::Register()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().connect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().disconnect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
}

void NodeEventHandler::OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	//	// Log that we have received a RegisterGame request
	//LOG_INFO << "Received RegisterGame request.";

	//// Parse client and server addresses
	//const InetAddress clientAddr(request->rpc_client().ip(), request->rpc_client().port());
	//const InetAddress serverAddr(request->rpc_server().ip(), request->rpc_server().port());
	//const entt::entity gameNodeId{ request->scene_node_id() };

	//// Log client and server addresses along with game node ID
	//LOG_INFO << "Client address: " << clientAddr.toIpPort();
	//LOG_INFO << "Server address: " << serverAddr.toIpPort();
	//LOG_INFO << "Game node ID: " << request->scene_node_id();

	//// Search for a matching client connection and register the game node
	//bool clientFound = false;
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
