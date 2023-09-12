#include "c2gate.h"

#include  <functional>
#include <memory>
#include <ranges>

#include "src/gate_server.h"
#include "src/util/game_registry.h"
#include "src/network/gs_node.h"
#include "src/network/login_node.h"
#include "src/pb/pbc/controller_service_service.h"
#include "src/pb/pbc/game_service_service.h"
#include "src/pb/pbc/login_service_service.h"
#include "src/pb/pbc/common_client_player_service.h"
#include "src/thread_local/gate_thread_local_storage.h"
#include "src/util/random.h"
#include "src/util/snow_flake.h"

ServerSequence32 g_server_sequence_;
extern std::unordered_set<uint32_t> g_c2s_service_id;

ClientReceiver::ClientReceiver(ProtobufCodec& codec, 
    ProtobufDispatcher& dispatcher)
    : codec_(codec),
      dispatcher_(dispatcher)
{
	dispatcher_.registerMessageCallback<ClientRequest>(
		std::bind(&ClientReceiver::OnRpcClientMessage, this, _1, _2, _3));
}

RpcClientPtr& ClientReceiver::GetLoginNode(uint64_t session_id)
{
    static RpcClientPtr null_session;
    const auto session_it = gate_tls.sessions().find(session_id);
    if (gate_tls.sessions().end() == session_it)
    {
        return null_session;
    }
    if (!session_it->second.ValidLogin())
    {
        session_it->second.login_node_id_ = FindValidLoginNodeId(session_id);
    }
    const auto login_node_it = gate_tls.login_nodes().find(session_it->second.login_node_id_);
    if (gate_tls.login_nodes().end() == login_node_it)
    {
        LOG_ERROR << "player found login server crash : " << session_it->second.login_node_id_;
        return null_session;
    }
    return login_node_it->second.login_session_;
}

uint32_t ClientReceiver::FindValidLoginNodeId(uint64_t session_id)
{
    const auto index = session_id % gate_tls.login_nodes().size();
	std::size_t i = 0;
	for (const auto& key : gate_tls.login_nodes() | std::views::keys)
	{
		if (i < index)
		{
			++i;
			continue;
		}
		return key;
	}
    return Session::kInvalidNodeId;
}

void ClientReceiver::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    //改包把消息发给其他玩家怎么办
    //todo 玩家没登录直接发其他消息，乱发消息
    if (!conn->connected())
    {
        const auto session_id = tcp_session_id(conn);
        //如果我没登录就发送其他协议到controller game server 怎么办
        {
            //此消息一定要发，不能值通过controller 的gw disconnect去发
            //比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
			LoginNodeDisconnectRequest rq;
			rq.set_session_id(session_id);
            const auto& session_login_node = GetLoginNode(session_id);
            if (nullptr != session_login_node)
            {
                session_login_node->CallMethod(LoginServiceDisconnectMsgId, rq);
            }			
        }
        // controller
        {
            GateDisconnectRequest rq;
            rq.set_session_id(session_id);
            g_gate_node->controller_node_session()->CallMethod(ControllerServiceGateDisconnectMsgId, rq);
        }
        gate_tls.sessions().erase(session_id);
    }
    else
    {
        auto session_id = g_server_sequence_.Generate();
        while (gate_tls.sessions().contains(session_id))
        {
            session_id = g_server_sequence_.Generate();
        }
        conn->setContext(session_id);
        Session session;
        session.conn_ = conn;
        gate_tls.sessions().emplace(session_id, std::move(session));
    }
}

void ClientReceiver::OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
    const RpcClientMessagePtr& request,
    muduo::Timestamp)
{
    const auto session_id = tcp_session_id(conn);
    const auto it = gate_tls.sessions().find(session_id);
	if (it == gate_tls.sessions().end())
	{
		return;
	}
    //todo msg id error
    if (g_c2s_service_id.contains(request->message_id()))
    {
		//检测玩家可以不可以发这个消息id过来给服务器
		auto gs = gate_tls.game_nodes().find(it->second.game_node_id_);
		if (gate_tls.game_nodes().end() == gs)
		{
            Tip(conn, 6);
			return;
		}
        GameNodeRpcClientRequest rq;
        rq.set_request(request->request());
        rq.set_session_id(session_id);
        rq.set_id(request->id());
        rq.set_message_id(request->message_id());
        gs->second.gs_session_->CallMethod(GameServiceClientSend2PlayerMsgId, rq);
        return;
    }
    else
    {
        //发往登录服务器,如果以后可能有其他服务器那么就特写一下,根据协议名字发送的对应服务器,
        RouteMsgStringRequest rq;
        rq.set_body(request->request());
        rq.set_session_id(session_id);
        rq.set_id(request->id());
        const auto message = rq.add_route_data_list();
        message->set_message_id(request->message_id());
        message->mutable_node_info()->CopyFrom(g_gate_node->node_info());
        const auto& login_node = GetLoginNode(session_id);
        if (nullptr == login_node)
        {
            return;
        }
        login_node->Route2Node(LoginServiceRouteNodeStringMsgMsgId, rq);
    }
}

void ClientReceiver::Tip(const muduo::net::TcpConnectionPtr& conn, uint32_t tip_id)
{
    TipsS2C tips;
    tips.mutable_tips()->set_id(tip_id);//tips_id.h 暂时写死，错误码不用编译
    MessageBody msg;
    msg.set_body(tips.SerializeAsString());
    msg.set_message_id(ClientPlayerCommonServicePushTipsS2CMsgId);
    g_gate_node->codec().send(conn, msg);
}


