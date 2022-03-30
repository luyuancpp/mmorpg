#include "c2gw.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/gateway_server.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity/entity.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/gate_player/gate_player_list.h"
#include "src/network/gs_node.h"
#include "src/return_code/error_code.h"
#include "src/server_common/rpc_closure.h"

#include "gw2l.pb.h"
#include "logic_proto/player_scene.pb.h"

using namespace common;
using namespace c2gw;
using namespace gateway;

static const uint64_t kEmptyId = 0;

namespace gateway
{
    extern std::unordered_set<std::string> g_open_player_services;

ClientReceiver::ClientReceiver(ProtobufCodec& codec, 
    ProtobufDispatcher& dispatcher, 
    RpcStubgw2l& gw2l_login_stub)
    : codec_(codec),
      dispatcher_(dispatcher),
      gw2l_login_stub_(gw2l_login_stub)
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

void ClientReceiver::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    //todo 玩家没登录直接发其他消息，乱发消息
    if (!conn->connected())
    {
        auto conn_id = uint64_t(conn.get());
        //断了线之后不能把消息串到别人的地方，串话
        //如果我没登录就发送其他协议到master game server 怎么办
        auto it = g_client_sessions_->find(conn_id);
        auto guid = it->second.guid_;        
        {
			gw2l::DisconnectRequest request;
			request.set_conn_id(conn_id);
			gw2l_login_stub_.CallMethod(request, &gw2l::LoginService_Stub::Disconnect);
        }
       
        // master
        {
            if (guid != common::kInvalidGuid)
            {
				msservice::DisconnectRequest request;
				request.set_conn_id(conn_id);
				request.set_guid(guid);
				//注意这里可能会有问题，如果发的connit 到ms 但是player id不对应怎么办?
				g_gateway_server->gw2ms_stub().CallMethod(request, &msservice::MasterNodeService_Stub::OnGwDisconnect);
            }           
        }

		// gs
		{
			if (guid != common::kInvalidGuid)
			{
				auto gs = g_gs_nodes.find(it->second.gs_node_id_);
				if (g_gs_nodes.end() != gs)
				{
					gw2gs::DisconnectRequest request;
					request.set_conn_id(conn_id);
					request.set_guid(guid);
					//注意这里可能会有问题，如果发的connit 到ms 但是player id不对应怎么办?
                    gs->second.gw2gs_stub_->CallMethod(request, &gw2gs::Gw2gsService_Stub::Disconnect);
				}
				
			}
		}
        g_client_sessions_->erase(conn_id);
    }
    else
    {
        //很极端情况下会有问题,如果走了一圈前面的人还没下线，在下一个id下线的瞬间又重用了,就会导致串话
        GateClient gc;
        gc.conn_ = conn;
        g_client_sessions_->emplace(uint64_t(conn.get()), gc);
    }
}

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginRpcReplied c(std::make_shared<LoginRpcReplied::element_type>(conn));
    c->s_rq_.set_account(message->account());
    c->s_rq_.set_password(message->password());
    c->s_rq_.set_conn_id(c->conn_id());
    c->s_rq_.set_gate_node_id(g_gateway_server->gate_node_id());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerLoginReplied,
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
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerCreatePlayerReplied,
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
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerEnterGameReplied,
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
		auto it = g_client_sessions_->find(cp->conn_id());
		if (it == g_client_sessions_->end())
		{
			return;
		}
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
	auto it = g_client_sessions_->find(uint64_t(conn.get()));
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
    if (g_open_player_services.find(request->service()) != g_open_player_services.end())
    {
		auto msg(std::make_shared<GsPlayerServiceRpcRplied::element_type>(conn));
        msg->s_rq_.set_request(request->SerializeAsString());
        msg->s_rq_.set_player_id(it->second.guid_);
        msg->c_rp_.set_id(request->id());
        msg->c_rp_.set_msg_id(request->msg_id());
        gs->second.gw2gs_stub_->CallMethod(&ClientReceiver::OnGsPlayerServiceReplied,
			msg,
			this,
			&gw2gs::Gw2gsService_Stub::PlayerService);
    }
}

void ClientReceiver::OnRpcClientReplied(ClientGSMessageReplied cp)
{
    codec_.send(cp->client_connection_, *cp->c_rp_);
}

void ClientReceiver::OnGsPlayerServiceReplied(GsPlayerServiceRpcRplied cp)
{
	auto it = g_client_sessions_->find(cp->s_rp_->conn_id());
	if (it == g_client_sessions_->end())
	{
		return;
	}
    auto& srp = cp->s_rp_;
    auto& crp = cp->c_rp_;
    if (it->second.guid_ != srp->player_id())
    {
        return;
    };
    crp.set_response(srp->response());
    codec_.send(cp->client_conn_, crp);
}

}


