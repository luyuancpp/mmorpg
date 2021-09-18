#include "login.h"

#include "muduo/base/Logging.h"

#include "src/luacpp/lua_client.h"
#include "src/service/service.h"

LoginModule::LoginModule(ClientService* service)
    : service_(service)
{
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

