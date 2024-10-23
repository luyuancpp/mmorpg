#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/client_player/player_state_attribute_sync.pb.h"


void InitEntitySyncServiceLua()
{
	tls_lua_state["EntitySyncServiceSyncBaseAttributeMessageId"] = 66;
	tls_lua_state["EntitySyncServiceSyncBaseAttributeIndex"] = 0;
	tls_lua_state["EntitySyncServiceSyncBaseAttribute"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(0);
	};

	tls_lua_state["EntitySyncServiceSyncAttributeMessageId"] = 67;
	tls_lua_state["EntitySyncServiceSyncAttributeIndex"] = 1;
	tls_lua_state["EntitySyncServiceSyncAttribute"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntitySyncService_Stub::descriptor()->method(1);
	};

}
