#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/common/login_service.pb.h"


void InitLoginServiceLua()
{
	tls_lua_state["LoginServiceLoginMessageId"] = 34;
	tls_lua_state["LoginServiceLoginIndex"] = 0;
	tls_lua_state["LoginServiceLogin"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return LoginService_Stub::descriptor()->method(0);
	};

	tls_lua_state["LoginServiceCreatePlayerMessageId"] = 33;
	tls_lua_state["LoginServiceCreatePlayerIndex"] = 1;
	tls_lua_state["LoginServiceCreatePlayer"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return LoginService_Stub::descriptor()->method(1);
	};

	tls_lua_state["LoginServiceEnterGameMessageId"] = 52;
	tls_lua_state["LoginServiceEnterGameIndex"] = 2;
	tls_lua_state["LoginServiceEnterGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return LoginService_Stub::descriptor()->method(2);
	};

	tls_lua_state["LoginServiceLeaveGameMessageId"] = 51;
	tls_lua_state["LoginServiceLeaveGameIndex"] = 3;
	tls_lua_state["LoginServiceLeaveGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return LoginService_Stub::descriptor()->method(3);
	};

	tls_lua_state["LoginServiceDisconnectMessageId"] = 55;
	tls_lua_state["LoginServiceDisconnectIndex"] = 4;
	tls_lua_state["LoginServiceDisconnect"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return LoginService_Stub::descriptor()->method(4);
	};

}
