#include "service_client.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/server_common/rpc_client_closure.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity_cast.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/gate_player/gate_player_list.h"
#include "src/return_code/error_code.h"

#include "gw2l.pb.h"

using namespace common;

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
    c->s_reqst_.set_account(message->account());
    c->s_reqst_.set_password(message->password());
    c->s_reqst_.set_connection_id(c->connection_id());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerLoginReplied,
        c, 
        this, 
        &gw2l::LoginService_Stub::Login);
}

void ClientReceiver::OnServerLoginReplied(LoginRpcReplied cp)
{
    auto& player_list = cp->s_resp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_resp_.add_players();
        p->set_guid(it.guid());
    }
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
                                    const CreatePlayerRequestPtr& message, 
                                    muduo::Timestamp)
{
    auto c(std::make_shared<CreatePlayeReplied::element_type>(conn));
    c->s_reqst_.set_connection_id(c->connection_id());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerCreatePlayerReplied,
        c, 
        this, 
        &gw2l::LoginService_Stub::CreatPlayer);
}

void ClientReceiver::OnServerCreatePlayerReplied(CreatePlayeReplied cp)
{
    auto& player_list = cp->s_resp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_resp_.add_players();
        p->set_guid(it.guid());
    }
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{
    auto c(std::make_shared<EnterGameRpcRplied::element_type>(conn));
    c->s_reqst_.set_connection_id(c->connection_id());
    c->s_reqst_.set_guid(message->guid());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerEnterGameReplied,
        c,
        this,
        &gw2l::LoginService_Stub::EnterGame);
}

void ClientReceiver::OnServerEnterGameReplied(EnterGameRpcRplied cp)
{
    //这里设置player id 还是会有串话问题
    auto& resp_ = cp->c_resp_;
    if (resp_.error().error_no() == RET_OK)
    {
        auto it = g_client_sessions_->find(cp->connection_id());
        if (it == g_client_sessions_->end())
        {
            return;
        }
        it->second.node_id_ = cp->s_resp_->gs_node_id();
        it->second.guid_ = cp->s_resp_->guid();
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

}


