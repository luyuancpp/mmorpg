#include "lua_module.h"

#include "muduo/base/Logging.h"

#include "src/client.h"
#include "src/util/file2string.h"

struct PlayerId {
public:
    thread_local static uint64_t guid;
};
thread_local uint64_t PlayerId::guid = 100;

void pb2sol2();
void InitServiceLua();

void LogInfo(const std::string& s)
{
    LOG_INFO << s;
}

void LogError(const std::string& s)
{
    LOG_ERROR << s;
}

void InitLua()
{
    g_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);
    g_lua.new_usertype<ClientService>("player", "sendraw",
			sol::as_function(&ClientService::SendOhter),
            "send",
            sol::as_function(&ClientService::Send));
    pb2sol2();

    g_lua.set_function("LogInfo", LogInfo);
    g_lua.set_function("LogError", LogError);

    g_lua.new_usertype<PlayerId>("PlayerId",
        "guid",
        sol::var(PlayerId::guid));
    InitServiceLua();
}

