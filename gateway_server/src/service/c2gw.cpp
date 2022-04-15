#include "c2gw.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/gateway_server.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/network/gate_player_list.h"
#include "src/network/gs_node.h"
#include "src/network/login_node.h"
#include "src/return_code/error_code.h"
#include "src/network/rpc_closure.h"

#include "login_node.pb.h"

using namespace common;
using namespace c2gw;
using namespace gateway;

std::unordered_set<common::Guid> g_connected_ids;
common::ServerSequence g_server_sequence_;

extern std::unordered_set<uint32_t> g_open_player_msgids;

namespace gateway
{
  
ClientReceiver::ClientReceiver(ProtobufCodec& codec, 
    ProtobufDispatcher& dispatcher)
    : codec_(codec),
      dispatcher_(dispatcher)
{
    dispatcher_.registerMessageCallback<LoginRequest>(
        std::bind(&ClientReceiver::OnLogin, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<CreatePlayerRequest>(
        std::bind(&ClientReceiver::OnCreatePlayer, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameRequest>(
        std::bind(&ClientReceiver::OnEnterGame, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<LeaveGameRequest>(
        std::bind(&ClientReceiver::OnLeaveGame, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<ClientRequest>(
		std::bind(&ClientReceiver::OnRpcClientMessage, this, _1, _2, _3));
}

gateway::ClientReceiver::RpcStubgw2l& ClientReceiver::login_stub()
{
    return *g_login_nodes.begin()->second.login_stub_;
}

void ClientReceiver::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    //todo 玩家没登录直接发其他消息，乱发消息
    if (!conn->connected())
    {
        auto conn_id = tcp_conn_id(conn);
        //断了线之后不能把消息串到别人的地方，串话
        //如果我没登录就发送其他协议到master game server 怎么办
        auto it = g_client_sessions_->find(conn_id);
        auto guid = it->second.guid_;        
        {
			gw2l::DisconnectRequest request;
			request.set_conn_id(conn_id);
			login_stub().CallMethod(request, &gw2l::LoginService_Stub::Disconnect);
        }
        // master
        {
            if (guid != common::kInvalidGuid)
            {
				msservice::DisconnectRequest request;
                request.set_gate_node_id(g_gateway_server->gate_node_id());
				request.set_guid(guid);
				//注意这里可能会有问题，如果发的connid 到ms 但是player id不对应怎么办?
				g_gateway_server->gw2ms_stub().CallMethod(request, &msservice::MasterNodeService_Stub::OnGwDisconnect);
            }           
        }
        g_client_sessions_->erase(conn_id);
        if (conn.use_count() == 1)//两处，一处是rpc server 返回,返回证明已经断开，而且该连接没有消息要处理了
        { 
            g_connected_ids.erase(conn_id); 
        }
    }
    else
    {
        auto id = g_server_sequence_.Generate();
        while (g_connected_ids.find(id) != g_connected_ids.end())
        {
            id = g_server_sequence_.Generate();
        }
        conn->setContext(id);
        GateClient gc;
        gc.conn_ = conn;
        g_client_sessions_->emplace(id, std::move(gc));
    }
}

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginRpcReplied c(std::make_shared<LoginRpcReplied::element_type>(conn));
    c->s_rq_.set_account(std::move(message->account()));
    c->s_rq_.set_password(std::move(message->password()));
    c->s_rq_.set_conn_id(c->conn_id());
    c->s_rq_.set_gate_node_id(g_gateway_server->gate_node_id());
    login_stub().CallMethod(&ClientReceiver::OnServerLoginReplied,
        c, 
        this, 
        &gw2l::LoginService_Stub::Login);
}

void ClientReceiver::OnServerLoginReplied(LoginRpcReplied cp)
{
    auto& player_list = cp->s_rp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_rp_.add_players();
        p->set_guid(it.guid());
    }
    codec_.send(cp->client_conn_, cp->c_rp_);
}

void ClientReceiver::OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
                                    const CreatePlayerRequestPtr& message, 
                                    muduo::Timestamp)
{
    auto c(std::make_shared<CreatePlayeReplied::element_type>(conn));
    c->s_rq_.set_conn_id(c->conn_id());
    login_stub().CallMethod(&ClientReceiver::OnServerCreatePlayerReplied,
        c, 
        this, 
        &gw2l::LoginService_Stub::CreatPlayer);
}

void ClientReceiver::OnServerCreatePlayerReplied(CreatePlayeReplied cp)
{
    auto& player_list = cp->s_rp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_rp_.add_players();
        p->set_guid(it.guid());
    }
    codec_.send(cp->client_conn_, cp->c_rp_);
}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{
    auto c(std::make_shared<EnterGameRpcRplied::element_type>(conn));
    c->s_rq_.set_conn_id(c->conn_id());
    c->s_rq_.set_guid(message->guid());
    login_stub().CallMethod(&ClientReceiver::OnServerEnterGameReplied,
        c,
        this,
        &gw2l::LoginService_Stub::EnterGame);
}

void ClientReceiver::OnServerEnterGameReplied(EnterGameRpcRplied cp)
{
    //这里设置player id 还是会有串话问题，断线以后重新上来一个新的玩家，同一个connection，到时候可以再加个token判断  
    auto& resp_ = cp->s_rp_;
    if (resp_->error().error_no() == RET_OK)//进入游戏有错误，直接返回给客户端
    {
        return;
    }      
	cp->c_rp_.mutable_error()->set_error_no(resp_->error().error_no());
	codec_.send(cp->client_conn_, cp->c_rp_);
	return;
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
	auto it = g_client_sessions_->find(tcp_conn_id(conn));
	if (it == g_client_sessions_->end())
	{
		return;
	}
    //检测玩家可以不可以发这个消息id过来给服务器
    auto gs = g_gs_nodes.find(it->second.gs_node_id_);
    if (g_gs_nodes.end() == gs)
    {
        //todo client error;
        return;
    }
    //todo msg id error
    if (g_open_player_msgids.find(request->msg_id()) != g_open_player_msgids.end())
    {
		auto msg(std::make_shared<GsPlayerServiceRpcRplied::element_type>(conn));
        msg->s_rq_.set_request(request->request());
        msg->s_rq_.set_player_id(it->second.guid_);
        msg->s_rq_.set_msg_id(request->msg_id());
        msg->c_rp_.set_id(request->id());
        msg->c_rp_.set_msg_id(request->msg_id());
        gs->second.gs_stub_->CallMethod(&ClientReceiver::OnGsPlayerServiceReplied,
			msg,
			this,
			&gsservice::GsService_Stub::GwPlayerService);
    }
}

void ClientReceiver::OnGsPlayerServiceReplied(GsPlayerServiceRpcRplied cp)
{
    auto& crp = cp->c_rp_;
    crp.set_response(std::move(cp->s_rp_->response()));
    codec_.send(cp->client_conn_, crp);
}

}


