#include "server_event_handler.h"
#include "proto/logic/event/server_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
#include "scene_node.h"
#include "network/rpc_client.h"
#include "service_info/centre_service_service_info.h"
#include "scene/system/game_node_scene_system.h"
#include "scene/system/scene_system.h"
#include "network/node_message_utils.h"
#include <proto/common/node.pb.h>
///<<< END WRITING YOUR CODE


void ServerEventHandler::Register()
{
    tls.dispatcher.sink<OnConnect2CentrePbEvent>().connect<&ServerEventHandler::OnConnect2CentrePbEventHandler>();
    tls.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
    tls.dispatcher.sink<OnServerStart>().connect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::UnRegister()
{
    tls.dispatcher.sink<OnConnect2CentrePbEvent>().disconnect<&ServerEventHandler::OnConnect2CentrePbEventHandler>();
    tls.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
    tls.dispatcher.sink<OnServerStart>().disconnect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::OnConnect2CentrePbEventHandler(const OnConnect2CentrePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	InitSceneNodeRequest request;
	request.set_node_id(gNode->GetNodeId());
	request.set_scene_node_type(gNode->GetNodeInfo().scene_node_type());
    CallRemoteMethodOnClient(CentreInitSceneNodeMessageId, request, entt::to_integral(event.entity()), eNodeType::CentreNodeService);

	entt::entity centreNodeId = entt::to_entity(event.entity());
	GameNodeSceneSystem::RegisterAllSceneToCentre(centreNodeId);
///<<< END WRITING YOUR CODE

}

void ServerEventHandler::OnConnect2LoginHandler(const OnConnect2Login& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void ServerEventHandler::OnServerStartHandler(const OnServerStart& event)
{
///<<< BEGIN WRITING YOUR CODE
    SceneUtil::SetSequenceNodeId(gNode->GetNodeId());
    GameNodeSceneSystem::InitializeNodeScenes();
///<<< END WRITING YOUR CODE

}
