#include <sol/sol.hpp>
#include "muduo/base/Logging.h"
#include "src/util/file2string.h"
#include "src/game_logic/thread_local/thread_local_storage_lua.h"
void InitServiceLua()
{
	std::string contents;
	contents = common::File2String("script/lua/service/client.lua");
	{
		auto r = tls_lua_state.script(contents);
		if (!r.valid())
		{
			sol::error err = r;
			LOG_FATAL << err.what();
		}
	}
	contents = common::File2String("script/lua/service/c_common_client_player.lua");
	{
		auto r = tls_lua_state.script(contents);
		if (!r.valid())
		{
			sol::error err = r;
			LOG_FATAL << err.what();
		}
	}
	contents = common::File2String("script/lua/service/c_scene_client_player.lua");
	{
		auto r = tls_lua_state.script(contents);
		if (!r.valid())
		{
			sol::error err = r;
			LOG_FATAL << err.what();
		}
	}
	contents = common::File2String("script/lua/service/c_team_client_player.lua");
	{
		auto r = tls_lua_state.script(contents);
		if (!r.valid())
		{
			sol::error err = r;
			LOG_FATAL << err.what();
		}
	}

}
