#include "controller_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene.h"
#include "src/pb/pbc/lobby_scene_service.h"
#include "src/system/gs_scene_system.h"
#include "src/game_config/deploy_json.h"
#include "src/game_server.h"
///<<< END WRITING YOUR CODE

void InitControllerServiceStartGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<CtrlStartGsResponse>(std::bind(&OnControllerServiceStartGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<StartLsResponse>(std::bind(&OnControllerServiceStartLsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<CtrlLoginAccountResponse>(std::bind(&OnControllerServiceLsLoginAccountRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<CtrlEnterGameResponese>(std::bind(&OnControllerServiceLsEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnControllerServiceGsPlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnControllerServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnControllerServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnControllerServiceStartGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlStartGsResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	if (GameConfig::GetSingleton().server_type() == kMainSceneServer)
	{
		for (int32_t i = 0; i < replied->scenes_info_size(); ++i)
		{
			CreateSceneBySceneInfoP param;
			param.scene_info_ = replied->scenes_info(i);
			GsSceneSystem::CreateSceneByGuid(param);
		}
		LOG_DEBUG << replied->DebugString();
	}
	else if (GameConfig::GetSingleton().server_type() == kMainSceneCrossServer)
	{
		GameConnectToControllerRequest rq;
		g_game_node->lobby_node()->CallMethod(LobbyServiceGameConnectToControllerMethod, &rq);
	}
///<<< END WRITING YOUR CODE
}

void OnControllerServiceStartLsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartLsResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceLsLoginAccountRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlLoginAccountResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceLsEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlEnterGameResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceGsPlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

