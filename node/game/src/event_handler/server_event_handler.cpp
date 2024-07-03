#include "server_event_handler.h"
#include "event_proto/server_event.pb.h"
#include "thread_local/thread_local_storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_node.h"
#include "network/rpc_client.h"
#include "service/centre_service_service.h"
#include "system/game_node_scene_system.h"
#include "system/scene/scene_system.h"
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

void ServerEventHandler::OnConnect2CentreHandler(const OnConnect2Centre& message)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity centre_id{ message.entity() };
    
    auto centre_node = tls.centre_node_registry.try_get<RpcClientPtr>(centre_id);
    if (centre_node == nullptr)
    {
        return;
    }
    auto& centre_local_addr = (*centre_node)->local_addr();
    RegisterGameRequest rq;
    rq.mutable_rpc_client()->set_ip(centre_local_addr.toIp());
    rq.mutable_rpc_client()->set_port(centre_local_addr.port());
    rq.mutable_rpc_server()->set_ip(g_game_node->GetNodeConf().ip());
    rq.mutable_rpc_server()->set_port(g_game_node->GetNodeConf().port());

    rq.set_server_type(g_game_node->GetNodeType());
    rq.set_game_node_id(g_game_node->GetNodeId());
    (*centre_node)->CallMethod(CentreServiceRegisterGameMsgId, rq);

    GameNodeSceneSystem::RegisterSceneToCentre();
	
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GameHandler(const OnConnect2Game& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GateHandler(const OnConnect2Gate& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2LoginHandler(const OnConnect2Login& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnServerStartHandler(const OnServerStart& message)
{
///<<< BEGIN WRITING YOUR CODE
    ScenesSystem::set_sequence_node_id(g_game_node->GetNodeId());
    GameNodeSceneSystem::InitNodeScene();
///<<< END WRITING YOUR CODE
}

