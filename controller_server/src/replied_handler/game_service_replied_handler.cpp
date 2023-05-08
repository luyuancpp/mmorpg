#include "game_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "src/comp/player_list.h"
#include "src/pb/pbc/service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/thread_local/controller_thread_local_storage.h"

///<<< END WRITING YOUR CODE

void InitGameServiceEnterGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGameServiceSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GameNodeRpcClientResponse>(std::bind(&OnGameServiceClientSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGameServiceCallPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGameServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGameServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameServiceSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceClientSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GameNodeRpcClientResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCallPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE

	auto it = controller_tls.player_list().find(replied->ex().player_id());
	if (it == controller_tls.player_list().end())
	{
		LOG_ERROR << "PlayerService player not found " << replied->ex().player_id() << ","
			<< replied->descriptor()->full_name() << " service " << replied->msg().service();
		return;
	}
	auto service_it = g_player_service_replieds.find(replied->msg().service());
	if (service_it == g_player_service_replieds.end())
	{
		LOG_ERROR << "PlayerService service not found " << replied->ex().player_id() << "," << replied->msg().service();
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(replied->msg().method());
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << replied->msg().method();
		//todo client error;
		return;
	}
	MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
	player_response->ParseFromString(replied->msg().body());
	serviceimpl->CallMethod(method, it->second, nullptr, boost::get_pointer(player_response));
///<<< END WRITING YOUR CODE
}

void OnGameServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

