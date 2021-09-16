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
        // bind as variable
        "player_id",
        sol::var(PlayerId::player_id));
}

void LoginModule::ReadyGo()
{
    LoginRequest request;
    sol::state& lua = LuaClient::GetSingleton().lua();
    request.set_password("lhl.2021");    
    lua.set("login", std::ref(request));
    LuaClient::GetSingleton().client().call();
    //LOG_INFO << request.DebugString().c_str();
    codec_.send(conn_, request);
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
