#include "lua_client.h"

#include "src/file2string/file2string.h"

#include "c2gw.pb.h"

using namespace common;

struct PlayerId {
public:
    thread_local static uint64_t guid;
};
thread_local uint64_t PlayerId::guid = 100;

void LuaClient::Init()
{
    lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);

    lua_.new_usertype<LoginRequest>("LoginRequest", "account",
        sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>));
    lua_.new_usertype<CreatePlayerRequest>("CreatePlayerRequest");
    lua_.new_usertype<EnterGameRequest>("EnterGameRequest", "guid",
        sol::property(&EnterGameRequest::set_guid, &EnterGameRequest::guid));
    lua_.new_usertype<LeaveGameRequest>("LeaveGameRequest");

    lua_.new_usertype<PlayerId>("PlayerId",
        "guid",
        sol::var(PlayerId::guid));

    auto contents = File2String("client/client.lua");
    lua_.script(contents);
}

