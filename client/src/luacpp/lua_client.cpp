#include "lua_client.h"

LuaClient::LuaClient()
{
    lua_.open_libraries(sol::lib::base, sol::lib::package);
    client_ =  lua_.load_file("client/client.lua");
}

