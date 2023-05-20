#include "controller_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/comp/account_player.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"
#include "src/network/node_info.h"
#include "src/network/route_system.h"
#include "src/util/defer.h"
#include "src/pb/pbc/service.h"

#include "src/pb/pbc/common_proto/database_service.pb.h"
#include "controller_service_service.h"

using PlayerPtr = std::shared_ptr<AccountPlayer>;
using ConnectionEntityMap = std::unordered_map<Guid, PlayerPtr>;

extern ConnectionEntityMap sessions_;
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitControllerServiceStartGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<CtrlStartGsResponse>(std::bind(&OnControllerServiceStartGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceGateConnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceGateLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceGatePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceGateDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<StartLsResponse>(std::bind(&OnControllerServiceStartLsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<CtrlLoginAccountResponse>(std::bind(&OnControllerServiceLsLoginAccountRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<CtrlEnterGameResponese>(std::bind(&OnControllerServiceLsEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceLsLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceLsDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnControllerServiceGsPlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceAddCrossServerSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnControllerServiceEnterGsSucceedRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnControllerServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnControllerServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnControllerServiceStartGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlStartGsResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceGateConnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceGateLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceGatePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceGateDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
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
    auto session_id = cl_tls.session_id();
    auto sit = sessions_.find(session_id);
    if (sit == sessions_.end())
    {
        replied->mutable_error()->set_id(kRetLoginSessionDisconnect);
        return;
    }
    //has data
    {
        auto& player = sit->second;
        auto ret = player->Login();
        if (ret != kRetOK)
        {
            replied->mutable_error()->set_id(ret);
            return;
        }
        g_login_node->redis_client()->Load(player->account_data(), sit->second->account());
        if (!player->account_data().password().empty())
        {
            LoginNodeLoginResponse message;
            message.mutable_account_player()->CopyFrom(player->account_data());
            player->OnDbLoaded();
            return;
        }
    }
    // database process
    DatabaseNodeLoginRequest rq;
    rq.set_account(replied->account());
    rq.set_session_id(session_id);

    Route2Node(kDatabaseNode, rq, ControllerServiceLsLoginAccountMethod);
///<<< END WRITING YOUR CODE
}

void OnControllerServiceLsEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlEnterGameResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceLsLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceLsDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceGsPlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceAddCrossServerSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceEnterGsSucceedRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnControllerServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	defer(cl_tls.set_current_session_id(kInvalidSessionId));
	cl_tls.set_current_session_id(replied->session_id());

    //函数返回前一定会执行的函数
    defer(cl_tls.set_next_route_node_type(UINT32_MAX));
    defer(cl_tls.set_next_route_node_id(UINT32_MAX));
    defer(cl_tls.set_current_session_id(kInvalidSessionId));
    if (replied->route_data_list_size() <= 0)
    {
        LOG_ERROR << "msg list empty:" << replied->DebugString();
        return;
    }
    //todo find all service
    auto& route_data = replied->route_data_list(replied->route_data_list_size() - 1);
    auto sit = g_service_method_info.find(route_data.message_id());
    if (sit == g_service_method_info.end())
    {
        LOG_INFO << "service_method_id not found " << route_data.message_id();
        return;
    }
    const google::protobuf::MethodDescriptor* method = g_login_node->login_handler().GetDescriptor()->FindMethodByName(sit->second.method);
    if (nullptr == method)
    {
        LOG_ERROR << "method not found" << replied->DebugString() << "method name" << route_data.method();
        return;
    }
    //当前节点的请求信息
    std::shared_ptr<google::protobuf::Message> current_node_response(g_login_node->login_handler().GetResponsePrototype(method).New());
    if (!current_node_response->ParseFromString(replied->body()))
    {
        LOG_ERROR << "invalid  body response" << replied->DebugString() << "method name" << route_data.method();
        return;
    }
    cl_tls.set_current_session_id(replied->session_id());
    //当前节点的真正回复的消息
    g_response_dispatcher.onProtobufMessage(conn, current_node_response, timestamp);

    auto mutable_replied = const_cast<::RouteMsgStringResponse*>(replied.get());
    mutable_replied->mutable_route_data_list()->RemoveLast();
    //已经是最后一个节点了
    if (mutable_replied->route_data_list_size() <= 0)
    {
        return;
    }
    //需要回复到前个节点
    auto prev_route_data = replied->route_data_list(replied->route_data_list_size() - 1);
    mutable_replied->set_body(cl_tls.route_msg_body());
    switch (cl_tls.next_route_node_type())
    {
        case kControllerNode:
        {

        }
        break;
        case kDatabaseNode:
        {
        }
        break;
        case kGateNode:
        {
            //todo test 节点不存在了消息会不会存留
        }
        break;

        default:
        {
            LOG_ERROR << "route to next node type error " << replied->DebugString() << "," << cl_tls.next_route_node_type();
        }
        break;
    }
///<<< END WRITING YOUR CODE
}

void OnControllerServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

