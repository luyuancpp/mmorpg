#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/logic/server_player/centre_player.pb.h"


void InitCentrePlayerServiceLua()
{
	tls_lua_state["CentrePlayerServiceTestMessageId"] = 6;
	tls_lua_state["CentrePlayerServiceTestIndex"] = 0;
	tls_lua_state["CentrePlayerServiceTest"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentrePlayerService_Stub::descriptor()->method(0);
	};

}
