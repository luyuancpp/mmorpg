#include "c2gate.h"

#include  <functional>
#include <memory>
#include <ranges>
#include <algorithm>

#include "gate_node.h"
#include "util/game_registry.h"
#include "network/gate_session.h"
#include "service/centre_service_service.h"
#include "service/game_service_service.h"
#include "service/common_client_player_service.h"
#include "thread_local/thread_local_storage_gate.h"
#include "util/random.h"
#include "util/snow_flake.h"
#include "service/login_service_service.h"
#include "grpc/request/login_grpc_request.h"
#include "global_value/gate_node_sequence.h"

#include "tip_code_proto/common_tip_code.pb.h"

extern std::unordered_set<uint32_t> g_c2s_service_id;

ClientReceiver::ClientReceiver(ProtobufCodec& codec, 
    ProtobufDispatcher& dispatcher)
    : codec_(codec),
      dispatcher_(dispatcher)
{
	dispatcher_.registerMessageCallback<ClientRequest>(
		std::bind(&ClientReceiver::OnRpcClientMessage, this, _1, _2, _3));
}

entt::entity ClientReceiver::GetLoginNode(uint64_t session_uid)
{
    auto it = tls_gate.sessions().find(session_uid);
    if (it == tls_gate.sessions().end())
    {
        return entt::null;
    }
    auto& session = it->second;
    if (!session.HasLoginNodeId())
    {
        auto login_node_it = tls_gate.login_consisten_node().get_by_hash(session_uid);
        if (tls_gate.login_consisten_node().end() == login_node_it)
        {
            LOG_ERROR << "player login server not found session id : " << session_uid;
            return entt::null;
        }
        //考虑中间一个login服务关了，原来的login服务器处理到一半，新的login处理不了
        session.login_node_id_ = entt::to_integral(login_node_it->second);
    }
    const auto login_node_it = tls_gate.login_consisten_node().get_node_value(session.login_node_id_);
    if (tls_gate.login_consisten_node().end() == login_node_it)
    {
        LOG_ERROR << "player found login server crash : " << session.login_node_id_;
        session.login_node_id_ = kInvalidNodeId;
        return entt::null;
    }
    return login_node_it->second;
}

void ClientReceiver::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    //改包把消息发给其他玩家怎么办
    //todo 玩家没登录直接发其他消息，乱发消息
    if (!conn->connected())
    {
        const auto session_uid = entt::to_integral(tcp_session_id(conn));
        //如果我没登录就发送其他协议到controller game server 怎么办
        {
            //此消息一定要发，不能值通过controller 的gw disconnect去发
            //比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
            if (const auto& login_node = GetLoginNode(session_uid);
                entt::null != login_node)
            {
                //todo 
                //LoginNodeDisconnectRequest rq;
                //rq.set_session_id(session_id);
                //session_login_node->CallMethod(LoginServiceDisconnectMsgId, rq);
            }			
        }
        // centre
        {
            GateSessionDisconnectRequest rq;
            rq.set_session_id(session_uid);
            g_gate_node->zone_centre_node()->CallMethod(CentreServiceGateSessionDisconnectMsgId, rq);
        }
        tls_gate.sessions().erase(session_uid);
    }
    else
    {
        auto session_id = g_node_sequence_.Generate();
        while (tls_gate.sessions().contains(session_id))
        {
            session_id = g_node_sequence_.Generate();
        }
        conn->setContext(session_id);
        Session session;
        session.conn_ = conn;
        tls_gate.sessions().emplace(session_id, std::move(session));
    }
}

void ClientReceiver::OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
    const RpcClientMessagePtr& request,
    muduo::Timestamp)
{
    auto session_id = tcp_session_id(conn);
    auto it = tls_gate.sessions().find(session_id);
    if (it == tls_gate.sessions().end())
    {
        LOG_ERROR << "could not find session  " << conn.get() ;
        return ;
    }
    //todo msg id error
    if (g_c2s_service_id.contains(request->message_id()))
    {
		//检测玩家可以不可以发这个消息id过来给服务器
        entt::entity game_node_id{ it->second.game_node_id_ };
		if (!tls.game_node_registry.valid(game_node_id))
		{
            Tip(conn, kRetServerCrush);
			return;
		}
        auto game_node = tls.game_node_registry.get<RpcClientPtr>(game_node_id);
        GameNodeRpcClientRequest rq;
        rq.set_body(request->body());
        rq.set_session_id(session_id);
        rq.set_id(request->id());
        rq.set_message_id(request->message_id());
        game_node->CallMethod(GameServiceClientSend2PlayerMsgId, rq);
    }
    else
    {
        //发往登录服务器,如果以后可能有其他服务器那么就特写一下,根据协议名字发送的对应服务器,
        
        auto login_node = GetLoginNode(session_id);
        if (entt::null == login_node)
        {
            //todo 关掉连接
            return;
        }

        if (request->message_id() == LoginServiceLoginMsgId)
        {
            LoginC2LRequest rq;
            rq.mutable_session_info()->set_session_id(session_id);
            rq.mutable_client_msg_body()->ParseFromArray(
                request->body().data(), request->body().size());
            SendLoginC2LRequest(login_node, rq);
        }else if (request->message_id() == LoginServiceCreatePlayerMsgId)
        {
            CreatePlayerC2LRequest rq;
            rq.mutable_session_info()->set_session_id(session_id);
            rq.mutable_client_msg_body()->ParseFromArray(
                request->body().data(), request->body().size());
            SendCreatePlayerC2LRequest(login_node, rq);
        }
        else if (request->message_id() == LoginServiceEnterGameMsgId)
        {
            EnterGameC2LRequest rq;
            rq.mutable_session_info()->set_session_id(session_id);
            rq.mutable_client_msg_body()->ParseFromArray(
                request->body().data(), request->body().size());
            SendEnterGameC2LRequest(login_node, rq);
        }
        /*RouteMsgStringRequest rq;
        rq.set_body(request->body());
        rq.set_session_id(session_uid);
        rq.set_id(request->id());
        const auto message = rq.add_route_data_list();
        message->set_message_id(request->message_id());
        message->mutable_node_info()->CopyFrom(g_gate_node->node_info());*/
        //todo 
    }
}

void ClientReceiver::Tip(const muduo::net::TcpConnectionPtr& conn, uint32_t tip_id)
{
    TipS2C tips;
    tips.mutable_tips()->set_id(tip_id);
    MessageBody msg;
    msg.set_body(tips.SerializeAsString());
    msg.set_message_id(ClientPlayerCommonServicePushTipS2CMsgId);
    g_gate_node->codec().send(conn, msg);
}


