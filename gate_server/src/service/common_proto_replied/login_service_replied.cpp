#include "login_service_replied.h"

#include "src/gate_server.h"

#include "c2gate.pb.h"

void OnServerCreatePlayerReplied(const TcpConnectionPtr& conn, const CreatePlayerResponsePtr& replied, Timestamp timestamp)
{
    CreatePlayerResponse msg;
    auto& player_list = replied->account_player().simple_players().players();
    for (auto& it : player_list)
    {
        auto p = msg.add_players();
        p->set_player_id(it.player_id());
    }
    g_gate_node->codec().send(conn, msg);
}

void OnEnterGameReplied(const TcpConnectionPtr& conn, const EnterGameResponsePtr& replied, Timestamp timestamp)
{
    EnterGameResponse msg;
    msg.mutable_error()->CopyFrom(replied->error());
    g_gate_node->codec().send(conn, msg);
}


void OnServerLoginReplied(const TcpConnectionPtr& conn, const LoginLoginResponsePtr& replied, Timestamp timestamp)
{
    LoginResponse msg;
    auto& player_list = replied->account_player().simple_players().players();
    for (auto& it : player_list)
    {
        auto p = msg.add_players();
        p->set_player_id(it.player_id());
    }
    g_gate_node->codec().send(conn, msg);
}
