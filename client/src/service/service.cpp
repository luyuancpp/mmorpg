#include "service.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"
#include "src/client_entityid/client_entityid.h"
#include "src/game_logic/game_registry.h"
#include "src/luacpp/lua_client.h"

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
    sol::state& lua = LuaClient::GetSingleton().lua();
    lua["LoginRequest"]["Send"] = [this](const LoginRequest& o) {
        Send(o);
    };
    lua["ReadyGo"]();
    //auto f = lua.get<sol::function>("ReadyGo");
    //f();
}

void ClientService::OnLoginReplied(const muduo::net::TcpConnectionPtr& conn, 
                                   const LoginResponsePtr& message, 
                                   muduo::Timestamp)
{
    sol::state& lua = LuaClient::GetSingleton().lua();
    lua["CreatePlayerRequest"]["Send"] = [this](const CreatePlayerRequest& o) {
        Send(o);
    };

    if (message->players().empty())
    {
        lua["CreatePlayer"]();
        return;
    }
    lua["EnterGameRequest"]["Send"] = [this](const EnterGameRequest& o) {
        Send(o);
    };
    lua["EnterGame"](message->players(0).player_id());
}

void ClientService::OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn, 
    const CreatePlayerResponsePtr& message,
    muduo::Timestamp)
{
    sol::state& lua = LuaClient::GetSingleton().lua();
    lua["EnterGameRequest"]["Send"] = [this](const EnterGameRequest& o) {
        Send(o);
    };
    lua["EnterGame"](message->players(0).player_id());
}

void ClientService::OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const EnterGameResponsePtr& message,
    muduo::Timestamp)
{
    sol::state& lua = LuaClient::GetSingleton().lua();
    lua["LeaveGameRequest"]["Send"] = [this](const LeaveGameRequest& o) {
        Send(o);
    };
    lua["LeaveGame"]();
}

void ClientService::OnLeaveGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameResponsePtr& message, 
    muduo::Timestamp)
{
    timer_task_.RunAfter(1, std::bind(&ClientService::DisConnect, this));
}

void ClientService::DisConnect()
{
    client_.disconnect();
    auto& c = common::reg().get<uint32_t>(client::gAllFinish);
    --c; 
    if (c == 0)
    {
        timer_task_.RunAfter(5, std::bind(&EventLoop::quit, EventLoop::getEventLoopOfCurrentThread()));
    }
}

