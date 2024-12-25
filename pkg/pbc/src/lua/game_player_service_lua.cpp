#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/server_player/game_player.pb.h"


void InitGamePlayerServiceLua()
{
	tls_lua_state["GamePlayerServiceCentre2GsLoginMessageId"] = 20;
	tls_lua_state["GamePlayerServiceCentre2GsLoginIndex"] = 0;
	tls_lua_state["GamePlayerServiceCentre2GsLogin"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GamePlayerService_Stub::descriptor()->method(0);
	};

	tls_lua_state["GamePlayerServiceExitGameMessageId"] = 62;
	tls_lua_state["GamePlayerServiceExitGameIndex"] = 1;
	tls_lua_state["GamePlayerServiceExitGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GamePlayerService_Stub::descriptor()->method(1);
	};

}
