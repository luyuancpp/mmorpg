#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/logic/client_player/player_state_attribute_sync.pb.h"


void InitEntitySyncServiceLua()
{
	tls_lua_state["EntitySyncServiceSyncBaseAttributeMessageId"] = 66;
	tls_lua_state["EntitySyncServiceSyncBaseAttributeIndex"] = 0;
	tls_lua_state["EntitySyncServiceSyncBaseAttribute"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(0);
	};

	tls_lua_state["EntitySyncServiceSyncAttribute2FramesMessageId"] = 67;
	tls_lua_state["EntitySyncServiceSyncAttribute2FramesIndex"] = 1;
	tls_lua_state["EntitySyncServiceSyncAttribute2Frames"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(1);
	};

	tls_lua_state["EntitySyncServiceSyncAttribute5FramesMessageId"] = 68;
	tls_lua_state["EntitySyncServiceSyncAttribute5FramesIndex"] = 2;
	tls_lua_state["EntitySyncServiceSyncAttribute5Frames"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(2);
	};

	tls_lua_state["EntitySyncServiceSyncAttribute10FramesMessageId"] = 69;
	tls_lua_state["EntitySyncServiceSyncAttribute10FramesIndex"] = 3;
	tls_lua_state["EntitySyncServiceSyncAttribute10Frames"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(3);
	};

	tls_lua_state["EntitySyncServiceSyncAttribute30FramesMessageId"] = 71;
	tls_lua_state["EntitySyncServiceSyncAttribute30FramesIndex"] = 4;
	tls_lua_state["EntitySyncServiceSyncAttribute30Frames"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(4);
	};

	tls_lua_state["EntitySyncServiceSyncAttribute60FramesMessageId"] = 70;
	tls_lua_state["EntitySyncServiceSyncAttribute60FramesIndex"] = 5;
	tls_lua_state["EntitySyncServiceSyncAttribute60Frames"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(5);
	};

}
