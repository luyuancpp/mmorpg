#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/common/db_service.pb.h"


void InitAccountDBServiceLua()
{
	tls_lua_state["AccountDBServiceLoad2RedisMessageId"] = 22;
	tls_lua_state["AccountDBServiceLoad2RedisIndex"] = 0;
	tls_lua_state["AccountDBServiceLoad2Redis"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return AccountDBService_Stub::descriptor()->method(0);
	};

	tls_lua_state["AccountDBServiceSave2RedisMessageId"] = 8;
	tls_lua_state["AccountDBServiceSave2RedisIndex"] = 1;
	tls_lua_state["AccountDBServiceSave2Redis"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return AccountDBService_Stub::descriptor()->method(1);
	};

}
