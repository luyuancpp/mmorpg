#include "service.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"
#include "src/client_entityid/client_entityid.h"
#include "src/game_logic/game_registry.h"

ClientService::ClientService(ProtobufDispatcher& dispatcher, 
                             ProtobufCodec& codec, 
                             TcpClient& client) : codec_(codec), 
                                                  client_(client),
                                                  login_(codec_, client_, conn_),
                                                  dispatcher_(dispatcher)
{
    dispatcher_.registerMessageCallback<LoginResponse>(
        std::bind(&ClientService::OnLoginReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<CreatePlayerResponse>(
        std::bind(&ClientService::OnCreatePlayerReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameResponse>(
        std::bind(&ClientService::OnEnterGameReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<LeaveGameResponse>(
        std::bind(&ClientService::OnLeaveGameReplied, this, _1, _2, _3));
}

void ClientService::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    conn_ = conn;
}

void ClientService::OnDisconnect()
{

}

void ClientService::ReadyGo()
{
    login_.ReadyGo();
}

void ClientService::OnLoginReplied(const muduo::net::TcpConnectionPtr& conn, 
                                   const LoginResponsePtr& message, 
                                   muduo::Timestamp)
{
    if (message->players().empty())
    {
        login_.CreatePlayer();
        return;
    }
    player_id_ = message->players(0).player_id();
    login_.EnterGame(player_id_);
}

void ClientService::OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn, 
    const CreatePlayerResponsePtr& message,
    muduo::Timestamp)
{
    player_id_ = message->players(0).player_id();
    login_.EnterGame(player_id_);
}

void ClientService::OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const EnterGameResponsePtr& message,
    muduo::Timestamp)
{
    login_.LeaveGame();
}

void ClientService::OnLeaveGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameResponsePtr& message, 
    muduo::Timestamp)
{
    common::reg().get<CountDownLatch*>(client::ClientEntityId::gAllLeaveGame)->countDown();
}

