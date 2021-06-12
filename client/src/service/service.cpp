#include "service.h"

#include "muduo/base/Logging.h"



ClientService::ClientService(ProtobufDispatcher& dispatcher, 
                             ProtobufCodec& codec, 
                             TcpClient& client) : codec_(codec), 
                                                  client_(client),
                                                  login_(codec_, client_, conn_),
                                                  dispatcher_(dispatcher)
{
    dispatcher_.registerMessageCallback<LoginResponse>(
        std::bind(&ClientService::OnLoginReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<CreatePlayerRespone>(
        std::bind(&ClientService::OnCreatePlayerReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameRespone>(
        std::bind(&ClientService::OnEnterGameReplied, this, _1, _2, _3));
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
    const CreatePlayerResponePtr& message,
    muduo::Timestamp)
{
    player_id_ = message->players(0).player_id();
    login_.EnterGame(player_id_);
}

void ClientService::OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const EnterGameResponePtr& message,
    muduo::Timestamp)
{
    client_.disconnect();
}

