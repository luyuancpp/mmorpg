#include "lua_client.h"

#include "src/file2string/file2string.h"

#include "c2gw.pb.h"

using namespace common;

struct PlayerId {
public:
    static uint64_t player_id;
};
uint64_t PlayerId::player_id = 100;

LuaClient::LuaClient()
{
    lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);

    lua_.new_usertype<LoginRequest>("LoginRequest", "account",
        sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>));
    lua_.new_usertype<CreatePlayerRequest>("CreatePlayerRequest");
    lua_.new_usertype<EnterGameRequest>("EnterGameRequest", "player_id",
        sol::property(&EnterGameRequest::set_player_id, &EnterGameRequest::player_id));
    lua_.new_usertype<LeaveGameRequest>("LeaveGameRequest");

    lua_.new_usertype<PlayerId>("PlayerId",
        "player_id",
        sol::var(PlayerId::player_id));
    auto contents = common::File2String("client/client.lua");
    lua_.script(contents);
    //auto b = result.valid();
    //lua_["ReadyGo"]();
}

