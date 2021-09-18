#include "login.h"

#include <atomic>

#include "muduo/base/Logging.h"

#include "src/luacpp/lua_client.h"
#include "src/service/service.h"

struct PlayerId {
public:
    static uint64_t player_id;
};

uint64_t PlayerId::player_id = 100;

LoginModule::LoginModule(ClientService* service)
    : service_(service)
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
        service_->Send(o);
        //LOG_INFO << o.DebugString().c_str();
    };
    LuaClient::GetSingleton().client().call();
}

void LoginModule::CreatePlayer()
{
    CreatePlayerRequest request;
    service_->Send(request);
}

void LoginModule::EnterGame(uint64_t player_id)
{
    EnterGameRequest request;
    request.set_player_id(player_id);
    service_->Send(request);
}

void LoginModule::LeaveGame()
{
    LeaveGameRequest request;
    service_->Send(request);
}

