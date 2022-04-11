#include "lua_module.h"

#include "muduo/base/Logging.h"

#include "src/client.h"
#include "src/util/file2string.h"

#include "c2gw.pb.h"

using namespace common;
using namespace c2gw;

struct PlayerId {
public:
    thread_local static uint64_t guid;
};
thread_local uint64_t PlayerId::guid = 100;

namespace common
{
    void pb2sol2();
}

void InitLua()
{
    g_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);
    g_lua.new_usertype<ClientService>("player", "sendother",
			sol::as_function(&ClientService::SendOhter));
    pb2sol2();

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

