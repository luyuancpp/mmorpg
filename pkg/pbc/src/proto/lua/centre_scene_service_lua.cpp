#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/server/centre_scene.pb.h"


void InitCentreSceneServiceLua()
{
	tls_lua_state["CentreSceneServiceRegisterSceneMessageId"] = 29;
	tls_lua_state["CentreSceneServiceRegisterSceneIndex"] = 0;
	tls_lua_state["CentreSceneServiceRegisterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreSceneService_Stub::descriptor()->method(0);
	};

	tls_lua_state["CentreSceneServiceUnRegisterSceneMessageId"] = 27;
	tls_lua_state["CentreSceneServiceUnRegisterSceneIndex"] = 1;
	tls_lua_state["CentreSceneServiceUnRegisterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreSceneService_Stub::descriptor()->method(1);
	};

}
