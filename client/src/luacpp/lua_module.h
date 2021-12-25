#ifndef CLIENT_SRC_LUA_CPP_LUA_MODULE_H_
#define CLIENT_SRC_LUA_CPP_LUA_MODULE_H_

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

class LuaModule
{
public:
    static LuaModule& GetSingleton() { thread_local  LuaModule singleton; return singleton; }

    sol::state& lua() { return lua_; }

    void Init();

private:
    sol::state lua_;
};

#endif//CLIENT_SRC_LUA_CPP_LUA_MODULE_H_
