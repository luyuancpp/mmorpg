#include <sol/sol.hpp>
#include "muduo/base/Logging.h"
#include "src/util/file2string.h"
extern thread_local sol::state g_lua;
void InitServiceLua()
{
	std::string contents;
	contents = common::File2String("script/client/service/c_team_client_player.lua");
	{
		auto r = g_lua.script(contents);
		if (!r.valid())
		{
			sol::error err = r;
			LOG_FATAL << err.what();
		}
	}
	contents = common::File2String("script/client/service/c_scene_client_player.lua");
	{
		auto r = g_lua.script(contents);
		if (!r.valid())
		{
			sol::error err = r;
			LOG_FATAL << err.what();
		}
	}

}
