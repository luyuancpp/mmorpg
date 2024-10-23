#include <cstdint>

#include "thread_local/storage_lua.h"
#include "logic/client_player/player_state_attribute_sync.pb.h"


void InitEntityStateSyncServiceLua()
{
	tls_lua_state["EntityStateSyncServiceSyncBaseStateAttributeMessageId"] = 67;
	tls_lua_state["EntityStateSyncServiceSyncBaseStateAttributeIndex"] = 0;
	tls_lua_state["EntityStateSyncServiceSyncBaseStateAttribute"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return EntityStateSyncService_Stub::descriptor()->method(0);
	};

}
