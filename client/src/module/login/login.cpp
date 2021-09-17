#include "login.h"

#include <atomic>

#include "muduo/base/Logging.h"

#include "src/luacpp/lua_client.h"

struct PlayerId {
public:
    static uint64_t player_id;
};

uint64_t PlayerId::player_id = 100;

LoginModule::LoginModule(ProtobufCodec& codec, TcpClient& client, TcpConnectionPtr& conn)
    : codec_(codec),
    client_(client),
    conn_(conn)
{
    sol::state& lua = LuaClient::GetSingleton().lua();
    lua.new_usertype<LoginRequest>("LoginRequest", "account",
        sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>));
    lua.new_usertype<PlayerId>("PlayerId",
        "player_id",
        sol::var(PlayerId::player_id));
}

void LoginModule::ReadyGo()
{
    sol::state& lua = LuaClient::GetSingleton().lua();  
    lua["LoginRequest"]["Send"] = [this](const LoginRequest& o) {
        codec_.send(conn_, o);
        //LOG_INFO << o.DebugString().c_str();
    };
    LuaClient::GetSingleton().client().call();
}

void LoginModule::CreatePlayer()
{
    CreatePlayerRequest request;
    codec_.send(conn_, request);
}

void LoginModule::EnterGame(uint64_t player_id)
{
    EnterGameRequest request;
    request.set_player_id(player_id);
    codec_.send(conn_, request);
}

void LoginModule::LeaveGame()
{
    LeaveGameRequest request;
    codec_.send(conn_, request);
}

