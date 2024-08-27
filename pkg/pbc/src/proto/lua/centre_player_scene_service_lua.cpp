#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/server_player/centre_player_scene.pb.h"


void InitCentrePlayerSceneServiceLua()
{
	tls_lua_state["CentrePlayerSceneServiceEnterSceneMessageId"] = 1;
	tls_lua_state["CentrePlayerSceneServiceEnterSceneIndex"] = 0;
	tls_lua_state["CentrePlayerSceneServiceEnterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentrePlayerSceneService_Stub::descriptor()->method(0);
	};

	tls_lua_state["CentrePlayerSceneServiceLeaveSceneMessageId"] = 3;
	tls_lua_state["CentrePlayerSceneServiceLeaveSceneIndex"] = 1;
	tls_lua_state["CentrePlayerSceneServiceLeaveScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentrePlayerSceneService_Stub::descriptor()->method(1);
	};

	tls_lua_state["CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId"] = 4;
	tls_lua_state["CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteIndex"] = 2;
	tls_lua_state["CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerComplete"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentrePlayerSceneService_Stub::descriptor()->method(2);
	};

	tls_lua_state["CentrePlayerSceneServiceSceneInfoC2SMessageId"] = 5;
	tls_lua_state["CentrePlayerSceneServiceSceneInfoC2SIndex"] = 3;
	tls_lua_state["CentrePlayerSceneServiceSceneInfoC2S"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentrePlayerSceneService_Stub::descriptor()->method(3);
	};

}
