#ifndef CLIENT_SRC_LUA_CPP_LUA_CLIENT_H_
#define CLIENT_SRC_LUA_CPP_LUA_CLIENT_H_

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

class LuaClient
{
public:

    static LuaClient& GetSingleton() { static LuaClient singleton; return singleton; }

    sol::state& lua() { return lua_; }
    sol::load_result& client() { return client_; }

    void Init();

private:
    sol::state lua_;
    sol::load_result client_;
};

#endif//CLIENT_SRC_LUA_CPP_LUA_CLIENT_H_
