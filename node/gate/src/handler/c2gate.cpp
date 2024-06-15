#include "c2gate.h"

#include  <functional>
#include <memory>
#include <ranges>

#include "gate_node.h"
#include "util/game_registry.h"
#include "network/gate_session.h"
#include "service/centre_service_service.h"
#include "service/game_service_service.h"
#include "service/common_client_player_service.h"
#include "thread_local/gate_thread_local_storage.h"
#include "util/random.h"
#include "util/snow_flake.h"

#include "tip_code_proto/common_tip_code.pb.h"

NodeBit15Sequence g_server_sequence_;
extern std::unordered_set<uint32_t> g_c2s_service_id;

ClientReceiver::ClientReceiver(ProtobufCodec& codec, 
    ProtobufDispatcher& dispatcher)
    : codec_(codec),
      dispatcher_(dispatcher)
{
	dispatcher_.registerMessageCallback<ClientRequest>(
		std::bind(&ClientReceiver::OnRpcClientMessage, this, _1, _2, _3));
}

RpcClientPtr& ClientReceiver::GetLoginNode(uint64_t session_uid)
{
    static RpcClientPtr null_session;
    return null_session;
    //entt::entity session_id{ session_uid };
    //if (!tls.session_registry.valid(session_id))
    //{
    //    LOG_ERROR << "session id not found   " << session_uid;
    //    return null_session;
    //}
    //auto session = tls.session_registry.try_get<Session>(session_id);
    //if (nullptr == session)
    //{
    //    LOG_ERROR << "session id not found   " << session_uid;
    //    return null_session;
    //}
    //
    //if (!session->HasLoginNodeId())
    //{
    //    auto login_node_it = gate_tls.login_nodes().get_by_hash(session_uid);
    //    if (gate_tls.login_nodes().end() == login_node_it)
    //    {
    //        LOG_ERROR << "player login server not found session id : " << session_uid;
    //        return null_session;
    //    }
    //    //考虑中间一个login服务关了，原来的login服务器处理到一半，新的login处理不了
    //    session->login_node_id_ = login_node_it->first;
    //}
    //auto login_node_id = session->login_node_id_;
    //const auto login_node_it = gate_tls.login_nodes().get_by_id(login_node_id);
    //if (gate_tls.login_nodes().end() == login_node_it)
    //{
    //    session->login_node_id_ = kInvalidNodeId;
    //    LOG_ERROR << "player found login server crash : " << session->login_node_id_;
    //    return null_session;
    //}
    //return login_node_it->second.login_session_;
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
            if (const auto& session_login_node = GetLoginNode(session_uid);
                nullptr != session_login_node)
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
        Destroy(tls.session_registry, entt::entity{ session_uid });
    }
    else
    {
        auto session_uid = g_server_sequence_.Generate();
        while (tls.session_registry.valid(entt::entity{session_uid}))
        {
            session_uid = g_server_sequence_.Generate();
        }
        entt::entity to_session_id{ session_uid };
        auto session_id = tls.session_registry.create(to_session_id);
        if (session_id != session_id)
        {
            LOG_ERROR << " create session ";
            return;
        }
        conn->setContext(session_id);
        auto& session = 
            tls.session_registry.emplace<Session>(session_id);
        session.conn_ = conn;

    }
}

void ClientReceiver::OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
    const RpcClientMessagePtr& request,
    muduo::Timestamp)
{
    entt::entity session_id = tcp_session_id(conn);
    if (!tls.session_registry.valid(session_id))
    {
        LOG_ERROR << "could not find session  " << conn.get() ;
        return ;
    }
    auto session_uid = entt::to_integral(session_id);
    auto session = tls.session_registry.get<Session>(session_id);
    //todo msg id error
    if (g_c2s_service_id.contains(request->message_id()))
    {
		//检测玩家可以不可以发这个消息id过来给服务器
        entt::entity game_node_id{ session.game_node_id_ };
		if (!tls.game_node_registry.valid(game_node_id))
		{
            Tip(conn, kRetServerCrush);
			return;
		}
        auto game_node = tls.game_node_registry.get<RpcClientPtr>(game_node_id);
        GameNodeRpcClientRequest rq;
        rq.set_body(request->body());
        rq.set_session_id(session_uid);
        rq.set_id(request->id());
        rq.set_message_id(request->message_id());
        game_node->CallMethod(GameServiceClientSend2PlayerMsgId, rq);
    }
    else
    {
        //发往登录服务器,如果以后可能有其他服务器那么就特写一下,根据协议名字发送的对应服务器,
        RouteMsgStringRequest rq;
        rq.set_body(request->body());
        rq.set_session_id(session_uid);
        rq.set_id(request->id());
        const auto message = rq.add_route_data_list();
        message->set_message_id(request->message_id());
        message->mutable_node_info()->CopyFrom(g_gate_node->node_info());
        const auto& login_node = GetLoginNode(session_uid);
        if (nullptr == login_node)
        {
            return;
        }
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


