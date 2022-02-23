#ifndef CLIENT_SRC_LUA_CPP_LUA_MODULE_H_
#define CLIENT_SRC_LUA_CPP_LUA_MODULE_H_

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

void InitLua();

extern thread_local sol::state thread_lua_;

#endif//CLIENT_SRC_LUA_CPP_LUA_MODULE_H_
