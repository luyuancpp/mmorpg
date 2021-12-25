#include "service.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"

#include "src/luacpp/lua_module.h"

using namespace common;

ClientService::ClientService(ProtobufDispatcher& dispatcher,
                             ProtobufCodec& codec, 
                             TcpClient& client) : codec_(codec), 
                                                  client_(client),
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
    auto& lua = LuaModule::GetSingleton().lua();
    lua["LoginRequest"]["Send"] = [this](LoginRequest& request) ->void
    {
        this->codec_.send(this->conn_, request);
    };
   lua["ReadyGo"]();
}

void ClientService::OnLoginReplied(const muduo::net::TcpConnectionPtr& conn, 
                                   const LoginResponsePtr& message, 
                                   muduo::Timestamp)
{
    if (message->players().empty())
    {        
        auto& lua = LuaModule::GetSingleton().lua();
        lua["CreatePlayerRequest"]["Send"] = [this](CreatePlayerRequest& request) ->void
        {
            this->codec_.send(this->conn_, request);
        };
        lua["CreatePlayer"]();
        return;
    }
    EnterGame(message->players(0).guid());   
}

void ClientService::OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn, 
    const CreatePlayerResponsePtr& message,
    muduo::Timestamp)
{
    EnterGame(message->players(0).guid());
}

void ClientService::OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const EnterGameResponsePtr& message,
    muduo::Timestamp)
{
    auto& lua = LuaModule::GetSingleton().lua();
    lua["LeaveGameRequest"]["Send"] = [this](LeaveGameRequest& request) ->void
    {
        this->codec_.send(this->conn_, request);
    };
    lua["LeaveGame"]();
}

void ClientService::OnLeaveGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameResponsePtr& message, 
    muduo::Timestamp)
{
    timer_task_.RunAfter(1, std::bind(&ClientService::DisConnect, this));
}

void ClientService::EnterGame(Guid guid)
{
    auto& lua = LuaModule::GetSingleton().lua();
    lua["EnterGameRequest"]["Send"] = [this](EnterGameRequest& request) ->void
    {
        this->codec_.send(this->conn_, request);
    };
    lua["EnterGame"](guid);
}

void ClientService::DisConnect()
{
    client_.disconnect();
}

