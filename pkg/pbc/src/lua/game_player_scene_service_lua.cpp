#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/logic/server_player/game_player_scene.pb.h"


void InitGamePlayerSceneServiceLua()
{
	tls_lua_state["GamePlayerSceneServiceEnterSceneMessageId"] = 46;
	tls_lua_state["GamePlayerSceneServiceEnterSceneIndex"] = 0;
	tls_lua_state["GamePlayerSceneServiceEnterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GamePlayerSceneService_Stub::descriptor()->method(0);
	};

	tls_lua_state["GamePlayerSceneServiceLeaveSceneMessageId"] = 56;
	tls_lua_state["GamePlayerSceneServiceLeaveSceneIndex"] = 1;
	tls_lua_state["GamePlayerSceneServiceLeaveScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GamePlayerSceneService_Stub::descriptor()->method(1);
	};

	tls_lua_state["GamePlayerSceneServiceEnterSceneS2CMessageId"] = 43;
	tls_lua_state["GamePlayerSceneServiceEnterSceneS2CIndex"] = 2;
	tls_lua_state["GamePlayerSceneServiceEnterSceneS2C"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GamePlayerSceneService_Stub::descriptor()->method(2);
	};

}
