#include "c2gw.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/server_common/rpc_client_closure.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity_cast.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/gate_player/gate_player_list.h"
#include "src/gs/gs_session.h"
#include "src/return_code/error_code.h"

#include "gw2l.pb.h"

using namespace common;
using namespace c2gw;
using namespace gateway;

static const uint64_t kEmptyId = 0;

namespace gateway
{

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
    if (!conn->connected())
    {
        auto connection_id = uint64_t(conn.get());
        //断了线之后不能把消息串到别人的地方，串话
        //如果我没登录就发送其他协议到master game server 怎么办
        gw2l::DisconnectRequest request;
        request.set_connection_id(connection_id);
        g_client_sessions_->erase(connection_id);
        gw2l_login_stub_.CallMethod(request,  &gw2l::LoginService_Stub::Disconnect);
    }
    else
    {
        //很极端情况下会有问题,如果走了一圈前面的人还没下线，在下一个id下线的瞬间又重用了,就会导致串话
        GateClient gc;
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
    c->s_rq_.set_connection_id(c->connection_id());
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
    codec_.send(cp->client_connection_, cp->c_rp_);
}

void ClientReceiver::OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
                                    const CreatePlayerRequestPtr& message, 
                                    muduo::Timestamp)
{
    auto c(std::make_shared<CreatePlayeReplied::element_type>(conn));
    c->s_rq_.set_connection_id(c->connection_id());
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
    codec_.send(cp->client_connection_, cp->c_rp_);
}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{
    auto c(std::make_shared<EnterGameRpcRplied::element_type>(conn));
    c->s_rq_.set_connection_id(c->connection_id());
    c->s_rq_.set_guid(message->guid());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerEnterGameReplied,
        c,
        this,
        &gw2l::LoginService_Stub::EnterGame);
}

void ClientReceiver::OnServerEnterGameReplied(EnterGameRpcRplied cp)
{
    //这里设置player id 还是会有串话问题，断线以后重新上来一个新的玩家，同一个connection，到时候可以再加个token判断  
    auto& resp_ = cp->c_rp_;
    if (resp_.error().error_no() == RET_OK)
    {
        auto it = g_client_sessions_->find(cp->connection_id());
        if (it == g_client_sessions_->end())
        {
            return;
        }
        it->second.gs_node_id_ = cp->s_rp_->gs_node_id();
        it->second.guid_ = cp->s_rp_->guid();
    }    
    codec_.send(cp->client_connection_, resp_);
}

void ClientReceiver::OnLeaveGame(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameRequestPtr& message, 
    muduo::Timestamp)
{
    LeaveGameResponse response;
    codec_.send(conn, response);
}

void ClientReceiver::OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
    const RpcClientMessagePtr& message,
    muduo::Timestamp)
{
	auto it = g_client_sessions_->find(uint64_t(conn.get()));
	if (it == g_client_sessions_->end())
	{
		return;
	}

    auto gs = g_gs_sesssion.GetSeesion(it->second.gs_node_id_);
    if (nullptr == gs)
    {
        //todo client error;
        return;
    }
    //todo msg id error
    const ::google::protobuf::ServiceDescriptor* c2gs_service = c2gs::C2GsService::descriptor();
    if (message->service() == c2gs_service->full_name())
    {
		//google::protobuf::Service* service = &c2gs_service_;
		//assert(service != NULL);
		//const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
		//const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message->method());
		//if (nullptr ==  method)
		//{
  //          //todo client error;
  //          return;          
		//}
  //      std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
  //      request->ParseFromString(message->request());
  //      google::protobuf::Message* response = service->GetResponsePrototype(method).New();        
		//auto c = std::make_shared<ClientGSMessageReplied::element_type>(conn);
		//c->c_rp_ = response;
  //      gs->gs_session_->CallMethod(*request, message->service(), message->method(), response,
  //          google::protobuf::NewCallback(this, &ClientReceiver::OnRpcClientReplied, c));
		auto c(std::make_shared<GsPlayerServiceRpcRplied::element_type>(conn));
        c->s_rq_.set_request(message->SerializeAsString());
		gs->gw2gs_stub_->CallMethod(&ClientReceiver::OnGsPlayerServiceReplied,
			c,
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

}

}


