#include "lua_module.h"

#include "muduo/base/Logging.h"
#include "src/file2string/file2string.h"
//#include "src/pb/pb2sol2/pb2sol2.h"

#include "c2gw.pb.h"

using namespace common;

struct PlayerId {
public:
    thread_local static uint64_t guid;
};
thread_local uint64_t PlayerId::guid = 100;

thread_local sol::state g_lua;

void InitLua()
{
    g_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);

    g_lua.new_usertype<LoginRequest>("LoginRequest", "account",
        sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>)); 
    g_lua.new_usertype<CreatePlayerRequest>("CreatePlayerRequest");
    g_lua.new_usertype<EnterGameRequest>("EnterGameRequest", "guid",
        sol::property(&EnterGameRequest::set_guid, &EnterGameRequest::guid));
    g_lua.new_usertype<LeaveGameRequest>("LeaveGameRequest");

    g_lua.new_usertype<PlayerId>("PlayerId",
        "guid",
        sol::var(PlayerId::guid));

    auto contents = File2String("script/client.lua");
    auto r = g_lua.script(contents);
    if (!r.valid())
    {
        sol::error err = r;
        LOG_FATAL << err.what();
    }    
}

