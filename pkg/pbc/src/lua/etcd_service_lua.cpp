#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/etcd/etcd.pb.h"


void InitKVLua()
{
	tls_lua_state["KVRangeMessageId"] = 77;
	tls_lua_state["KVRangeIndex"] = 0;
	tls_lua_state["KVRange"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return KV_Stub::descriptor()->method(0);
	};

	tls_lua_state["KVPutMessageId"] = 76;
	tls_lua_state["KVPutIndex"] = 1;
	tls_lua_state["KVPut"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return KV_Stub::descriptor()->method(1);
	};

	tls_lua_state["KVDeleteRangeMessageId"] = 78;
	tls_lua_state["KVDeleteRangeIndex"] = 2;
	tls_lua_state["KVDeleteRange"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return KV_Stub::descriptor()->method(2);
	};

	tls_lua_state["KVTxnMessageId"] = 79;
	tls_lua_state["KVTxnIndex"] = 3;
	tls_lua_state["KVTxn"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return KV_Stub::descriptor()->method(3);
	};

	tls_lua_state["KVCompactMessageId"] = 80;
	tls_lua_state["KVCompactIndex"] = 4;
	tls_lua_state["KVCompact"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return KV_Stub::descriptor()->method(4);
	};

}
