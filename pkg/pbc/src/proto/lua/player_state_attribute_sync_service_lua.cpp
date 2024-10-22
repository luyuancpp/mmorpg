#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/client_player/player_state_attribute_sync.pb.h"


void InitEntityStateSyncServiceLua()
{
	tls_lua_state["EntityStateSyncServiceSyncVelocityMessageId"] = 66;
	tls_lua_state["EntityStateSyncServiceSyncVelocityIndex"] = 0;
	tls_lua_state["EntityStateSyncServiceSyncVelocity"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntityStateSyncService_Stub::descriptor()->method(0);
	};

}
