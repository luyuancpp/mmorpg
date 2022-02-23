#include "lua_module.h"

#include "muduo/base/Logging.h"
#include "src/file2string/file2string.h"

#include "c2gw.pb.h"

using namespace common;

struct PlayerId {
public:
    thread_local static uint64_t guid;
};
thread_local uint64_t PlayerId::guid = 100;

thread_local sol::state thread_lua_;

void InitLua()
{
    thread_lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);

    thread_lua_.new_usertype<LoginRequest>("LoginRequest", "account",
        sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>));
    thread_lua_.new_usertype<CreatePlayerRequest>("CreatePlayerRequest");
    thread_lua_.new_usertype<EnterGameRequest>("EnterGameRequest", "guid",
        sol::property(&EnterGameRequest::set_guid, &EnterGameRequest::guid));
    thread_lua_.new_usertype<LeaveGameRequest>("LeaveGameRequest");

    thread_lua_.new_usertype<PlayerId>("PlayerId",
        "guid",
        sol::var(PlayerId::guid));

    auto contents = File2String("script/client.lua");
    auto r = thread_lua_.script(contents);
    if (!r.valid())
    {
        sol::error err = r;
        LOG_FATAL << err.what();
    }    
}

