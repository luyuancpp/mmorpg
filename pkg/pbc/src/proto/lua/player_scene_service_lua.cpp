#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/client_player/player_scene.pb.h"


void InitClientPlayerSceneServiceLua()
{
	tls_lua_state["ClientPlayerSceneServiceEnterSceneMessageId"] = 32;
	tls_lua_state["ClientPlayerSceneServiceEnterSceneIndex"] = 0;
	tls_lua_state["ClientPlayerSceneServiceEnterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(0);
	};

	tls_lua_state["ClientPlayerSceneServiceNotifyEnterSceneMessageId"] = 60;
	tls_lua_state["ClientPlayerSceneServiceNotifyEnterSceneIndex"] = 1;
	tls_lua_state["ClientPlayerSceneServiceNotifyEnterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(1);
	};

	tls_lua_state["ClientPlayerSceneServiceSceneInfoC2SMessageId"] = 23;
	tls_lua_state["ClientPlayerSceneServiceSceneInfoC2SIndex"] = 2;
	tls_lua_state["ClientPlayerSceneServiceSceneInfoC2S"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(2);
	};

	tls_lua_state["ClientPlayerSceneServiceNotifySceneInfoMessageId"] = 17;
	tls_lua_state["ClientPlayerSceneServiceNotifySceneInfoIndex"] = 3;
	tls_lua_state["ClientPlayerSceneServiceNotifySceneInfo"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(3);
	};

	tls_lua_state["ClientPlayerSceneServiceNotifyActorCreateMessageId"] = 50;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorCreateIndex"] = 4;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorCreate"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(4);
	};

	tls_lua_state["ClientPlayerSceneServiceNotifyActorDestroyMessageId"] = 21;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorDestroyIndex"] = 5;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorDestroy"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(5);
	};

	tls_lua_state["ClientPlayerSceneServiceNotifyActorListCreateMessageId"] = 16;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorListCreateIndex"] = 6;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorListCreate"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(6);
	};

	tls_lua_state["ClientPlayerSceneServiceNotifyActorListDestroyMessageId"] = 47;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorListDestroyIndex"] = 7;
	tls_lua_state["ClientPlayerSceneServiceNotifyActorListDestroy"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return ClientPlayerSceneService_Stub::descriptor()->method(7);
	};

}
