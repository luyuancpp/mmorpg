#include "server_event_handler.h"
#include "logic/event/server_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_node.h"
#include "network/rpc_client.h"
#include "service_info/centre_service_service_info.h"
#include "game_logic/scene/util/game_node_scene_system.h"
#include "scene/util/scene_system.h"
///<<< END WRITING YOUR CODE
void ServerEventHandler::Register()
{
	tls.dispatcher.sink<OnConnect2Centre>().connect<&ServerEventHandler::OnConnect2CentreHandler>();
	tls.dispatcher.sink<OnConnect2Game>().connect<&ServerEventHandler::OnConnect2GameHandler>();
	tls.dispatcher.sink<OnConnect2Gate>().connect<&ServerEventHandler::OnConnect2GateHandler>();
	tls.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
	tls.dispatcher.sink<OnServerStart>().connect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::UnRegister()
{
	tls.dispatcher.sink<OnConnect2Centre>().disconnect<&ServerEventHandler::OnConnect2CentreHandler>();
	tls.dispatcher.sink<OnConnect2Game>().disconnect<&ServerEventHandler::OnConnect2GameHandler>();
	tls.dispatcher.sink<OnConnect2Gate>().disconnect<&ServerEventHandler::OnConnect2GateHandler>();
	tls.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
	tls.dispatcher.sink<OnServerStart>().disconnect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::OnConnect2CentreHandler(const OnConnect2Centre& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity centreId{ event.entity() };

	auto centreNode = tls.centreNodeRegistry.try_get<RpcClientPtr>(centreId);
	if (centreNode == nullptr)
	{
		LOG_ERROR << "Centre node not found for entity: " << entt::to_integral(centreId);
		return;
	}

	auto& centreLocalAddr = (*centreNode)->local_addr();

	RegisterGameNodeRequest registerGameRequest;
	registerGameRequest.mutable_rpc_client()->set_ip(centreLocalAddr.toIp());
	registerGameRequest.mutable_rpc_client()->set_port(centreLocalAddr.port());
	registerGameRequest.mutable_rpc_server()->set_ip(gGameNode->GetNodeConf().ip());
	registerGameRequest.mutable_rpc_server()->set_port(gGameNode->GetNodeConf().port());
	registerGameRequest.set_server_type(gGameNode->GetNodeType());
	registerGameRequest.set_game_node_id(gGameNode->GetNodeId());

	LOG_INFO << "Sending RegisterGameRequest to centre node: " << entt::to_integral(centreId);
	(*centreNode)->CallRemoteMethod(CentreServiceRegisterGameNodeMessageId, registerGameRequest);

	GameNodeSceneSystem::RegisterSceneToCentre();
	
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GameHandler(const OnConnect2Game& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GateHandler(const OnConnect2Gate& event)
{
///<<< BEGIN WRITING YOUR CODE
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
    SceneUtil::SetSequenceNodeId(gGameNode->GetNodeId());
    GameNodeSceneSystem::InitializeNodeScenes();
///<<< END WRITING YOUR CODE
}

