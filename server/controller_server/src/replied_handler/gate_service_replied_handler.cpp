#include "gate_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "src/type_define/type_define.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/system/player_change_scene.h"
#include "src/util/game_registry.h"
#include "src/system/player_common_system.h"
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitGateServiceStartGSRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGateServiceStartGSRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGateServiceStopGSRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GateNodePlayerUpdateGameNodeResponese>(std::bind(&OnGateServicePlayerEnterGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGateServicePlayerMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGateServiceKickConnByControllerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGateServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGateServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGateServiceStartGSRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceStopGSRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServicePlayerEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GateNodePlayerUpdateGameNodeResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	///gate 更新gs,相应的gs可以往那个gate上发消息了
	///todo 中间返回是断开了
	entt::entity GetPlayerByConnId(uint64_t session_id);
	const auto player = GetPlayerByConnId(replied->session_id());
	if (entt::null == player)
	{
		LOG_ERROR << "player not found " << tls.registry.get<Guid>(player);
		return;
	}
	PlayerCommonSystem::OnGateUpdateGameNodeSucceed(player);
	PlayerChangeSceneSystem::SetChangeGsStatus(player, ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE
}

void OnGateServicePlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceKickConnByControllerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE



///<<< END WRITING YOUR CODE
}

void OnGateServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

