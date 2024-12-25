#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/server/game_scene.pb.h"


void InitGameSceneServiceLua()
{
	tls_lua_state["GameSceneServiceTestMessageId"] = 48;
	tls_lua_state["GameSceneServiceTestIndex"] = 0;
	tls_lua_state["GameSceneServiceTest"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameSceneService_Stub::descriptor()->method(0);
	};

}
