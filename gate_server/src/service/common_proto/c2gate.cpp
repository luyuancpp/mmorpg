#include "c2gate.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/gate_server.h"
#include "src/util/game_registry.h"
#include "src/network/gs_node.h"
#include "src/network/login_node.h"
#include "src/game_logic/tips_id.h"
#include "src/network/rpc_msg_route.h"
#include "src/thread_local/gate_thread_local_storage.h"
#include "src/util/random.h"
#include "src/pb/pbc/service_method/controller_servicemethod.h"
#include "src/pb/pbc/service_method/game_servicemethod.h"
#include "src/pb/pbc/service_method/login_servicemethod.h"

#include "login_service.pb.h"

ServerSequence32 g_server_sequence_;

extern std::unordered_set<uint32_t> g_open_player_msgids;

ClientReceiver::ClientReceiver(ProtobufCodec& codec, 
    ProtobufDispatcher& dispatcher)
    : codec_(codec),
      dispatcher_(dispatcher)
{
    
    dispatcher_.registerMessageCallback<CreatePlayerRequest>(
        std::bind(&ClientReceiver::OnCreatePlayer, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameRequest>(
        std::bind(&ClientReceiver::OnEnterGame, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<LeaveGameRequest>(
        std::bind(&ClientReceiver::OnLeaveGame, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<ClientRequest>(
		std::bind(&ClientReceiver::OnRpcClientMessage, this, _1, _2, _3));
}

RpcClientPtr& ClientReceiver::get_login_node()
{
    auto index = Random::GetSingleton().Rand<std::size_t>(0, gate_tls.login_nodes().size());
    std::size_t i = 0;
    for (auto& it : gate_tls.login_nodes())
    {
        if (i < index)
        {
            ++i;
            continue;
        }
        return it.second.login_session_;
    }
    return gate_tls.login_nodes().begin()->second.login_session_;
}

RpcClientPtr& ClientReceiver::get_login_node(uint64_t session_id)
{
    static RpcClientPtr empty_session;
    auto session_it = gate_tls.sessions().find(session_id);
    if (gate_tls.sessions().end() == session_it)
    {
        return empty_session;
    }
    if (!session_it->second.ValidLogin())
    {
        session_it->second.login_node_id_ = find_valid_login_node_id(session_id);
    }
    auto login_node_it = gate_tls.login_nodes().find(session_it->second.login_node_id_);
    if (gate_tls.login_nodes().end() == login_node_it)
    {
        LOG_ERROR << "player login server not found : " << session_it->second.login_node_id_;
        return empty_session;
    }
    return login_node_it->second.login_session_;
}

uint32_t ClientReceiver::find_valid_login_node_id(uint64_t session_id)
{
	auto index = session_id % gate_tls.login_nodes().size();
	std::size_t i = 0;
	for (auto& it : gate_tls.login_nodes())
	{
		if (i < index)
		{
			++i;
			continue;
		}
		return it.first;
	}
    return GateClient::kInvalidNodeId;
}

void ClientReceiver::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    //改包把消息发给其他玩家怎么办
    //todo 玩家没登录直接发其他消息，乱发消息
    if (!conn->connected())
    {
        auto session_id = tcp_session_id(conn);
        //如果我没登录就发送其他协议到controller game server 怎么办
        {
            //此消息一定要发，不能值通过controller 的gw disconnect去发
            //比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
			LoginNodeDisconnectRequest rq;
			rq.set_session_id(session_id);
			get_login_node(session_id)->CallMethod(LoginServiceDisconnectMethodDesc, &rq);
        }
        // controller
        {
            ControllerNodeDisconnectRequest rq;
            rq.set_session_id(session_id);
            g_gate_node->controller_node_session()->CallMethod(ControllerServiceOnGateDisconnectMethodDesc, &rq);
        }
        gate_tls.sessions().erase(session_id);
    }
    else
    {
        auto id = g_server_sequence_.Generate();
        while (gate_tls.sessions().find(id) != gate_tls.sessions().end())
        {
            id = g_server_sequence_.Generate();
        }
        conn->setContext(id);
        GateClient gc;
        gc.conn_ = conn;
        gate_tls.sessions().emplace(id, std::move(gc));
    }
}

void ClientReceiver::OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
                                    const CreatePlayerRequestPtr& message, 
                                    muduo::Timestamp)
{
    CreatePlayerC2lRequest rq;
    rq.set_session_id(tcp_session_id(conn));
    get_login_node(tcp_session_id(conn))->CallMethod(LoginServiceCreatPlayerMethodDesc, &rq);
}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{
    EnterGameC2LRequest rq;
    rq.set_session_id(tcp_session_id(conn));
    rq.set_player_id(message->player_id());
    get_login_node(tcp_session_id(conn))->CallMethod(LoginServiceEnterGameMethodDesc, &rq);
}

void ClientReceiver::OnLeaveGame(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameRequestPtr& message, 
    muduo::Timestamp)
{
    LeaveGameResponse response;
    codec_.send(conn, response);
}

void ClientReceiver::OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
    const RpcClientMessagePtr& request,
    muduo::Timestamp)
{
    auto session_id = tcp_session_id(conn);
	auto it = gate_tls.sessions().find(session_id);
	if (it == gate_tls.sessions().end())
	{
		return;
	}
    //todo msg id error
    if (g_open_player_msgids.find(request->msg_id()) != g_open_player_msgids.end())
    {
		//检测玩家可以不可以发这个消息id过来给服务器
		auto gs = g_game_node.find(it->second.gs_node_id_);
		if (g_game_node.end() == gs)
		{
			//todo client error;
			return;
		}

        GameNodeRpcClientRequest rq;
        rq.set_request(request->request());
        rq.set_session_id(session_id);
        rq.set_msg_id(request->msg_id());
        rq.set_id(request->id());
        gs->second.gs_session_->CallMethod(GameServiceClientSend2PlayerMethodDesc, &rq);
    }
    {
        RouteMsgStringRequest rq;
        rq.set_body(request->request());
        rq.set_session_id(session_id);
        auto msg = rq.add_msg_list();
        msg->mutable_node_info()->CopyFrom(g_gate_node->node_info());        
        get_login_node(session_id)->CallMethod(LoginServiceRouteNodeStringMsgMethodDesc, &rq);
    }
}



