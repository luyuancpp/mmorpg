#include "service_client.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/server_common/rpc_client_closure.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity_cast.h"
#include "src/game_logic/comp/player.hpp"
#include "src/gate_player/gate_player_list.h"
#include "src/return_code/return_notice_code.h"

#include "gw2l.pb.h"

using namespace common;

static const uint32_t kEmptyId = 0;

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
        auto connection_id = boost::any_cast<uint64_t>(conn->getContext());
        //断了线之后不能把消息串到别人的地方，串话
        //如果我没登录就发送其他协议到master game server 怎么办
        gw2l::DisconnectRequest request;
        request.set_connection_id(connection_id);
        gw2l_login_stub_.CallMethod(request,  &gw2l::LoginService_Stub::Disconnect);
        g_gate_clients_->erase(connection_id);
        conn->setContext(kEmptyId);
    }
    else
    {
        ++id_;
        while (id_ == kEmptyId || g_gate_clients_->find(id_) != g_gate_clients_->end())
        {
            ++id_;
        }
        //很极端情况下会有问题,如果走了一圈前面的人还没下线，在下一个id下线的瞬间又重用了,就会导致串话
        conn->setContext(id_);
        GateClient gc;
        g_gate_clients_->emplace(id_, gc);        
    }
}

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginCCPtr p(std::make_shared<LoginCC>(conn));
    p->s_reqst_.set_account(message->account());
    p->s_reqst_.set_password(message->password());
    p->s_reqst_.set_connection_id(p->connection_id());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerLoginReplied,
        p, 
        this, 
        &gw2l::LoginService_Stub::Login);
}

void ClientReceiver::OnServerLoginReplied(LoginCCPtr cp)
{
    auto& player_list = cp->s_resp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_resp_.add_players();
        p->set_player_id(it.player_id());
    }
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
                                    const CreatePlayerRequestPtr& message, 
                                    muduo::Timestamp)
{
    CreatePlayerCCPtr p(std::make_shared<CreatePlayerCC>(conn));
    p->s_reqst_.set_connection_id(p->connection_id());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerCreatePlayerReplied,
        p, 
        this, 
        &gw2l::LoginService_Stub::CreatPlayer);
}

void ClientReceiver::OnServerCreatePlayerReplied(CreatePlayerCCPtr cp)
{
    auto& player_list = cp->s_resp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_resp_.add_players();
        p->set_player_id(it.player_id());
    }
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{
    EnterGameCCPtr p(std::make_shared<EnterGameCC>(conn));
    p->s_reqst_.set_connection_id(p->connection_id());
    p->s_reqst_.set_player_id(message->player_id());
    gw2l_login_stub_.CallMethod(&ClientReceiver::OnServerEnterGameReplied,
        p,
        this,
        &gw2l::LoginService_Stub::EnterGame);
}

void ClientReceiver::OnServerEnterGameReplied(EnterGameCCPtr cp)
{
    //这里设置player id 还是会有串话问题
    auto& resp_ = cp->c_resp_;
    if (resp_.error().error_no() == RET_OK)
    {
        auto it = g_gate_clients_->find(cp->connection_id());
        if (cp->connection_id() != kEmptyId && it == g_gate_clients_->end())
        {
            assert(false);
            return;
        }
        it->second.player_id_ = cp->s_resp_->player_id();
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


