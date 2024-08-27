#include <cstdint>

#include "thread_local/storage_lua.h"
#include "common/centre_service.pb.h"


void InitCentreServiceLua()
{
	tls_lua_state["CentreServiceRegisterGameNodeMessageId"] = 54;
	tls_lua_state["CentreServiceRegisterGameNodeIndex"] = 0;
	tls_lua_state["CentreServiceRegisterGameNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(0);
	};

	tls_lua_state["CentreServiceRegisterGateNodeMessageId"] = 2;
	tls_lua_state["CentreServiceRegisterGateNodeIndex"] = 1;
	tls_lua_state["CentreServiceRegisterGateNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(1);
	};

	tls_lua_state["CentreServiceGatePlayerServiceMessageId"] = 36;
	tls_lua_state["CentreServiceGatePlayerServiceIndex"] = 2;
	tls_lua_state["CentreServiceGatePlayerService"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(2);
	};

	tls_lua_state["CentreServiceGateSessionDisconnectMessageId"] = 9;
	tls_lua_state["CentreServiceGateSessionDisconnectIndex"] = 3;
	tls_lua_state["CentreServiceGateSessionDisconnect"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(3);
	};

	tls_lua_state["CentreServiceLoginNodeAccountLoginMessageId"] = 19;
	tls_lua_state["CentreServiceLoginNodeAccountLoginIndex"] = 4;
	tls_lua_state["CentreServiceLoginNodeAccountLogin"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(4);
	};

	tls_lua_state["CentreServiceLoginNodeEnterGameMessageId"] = 53;
	tls_lua_state["CentreServiceLoginNodeEnterGameIndex"] = 5;
	tls_lua_state["CentreServiceLoginNodeEnterGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(5);
	};

	tls_lua_state["CentreServiceLoginNodeLeaveGameMessageId"] = 41;
	tls_lua_state["CentreServiceLoginNodeLeaveGameIndex"] = 6;
	tls_lua_state["CentreServiceLoginNodeLeaveGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(6);
	};

	tls_lua_state["CentreServiceLoginNodeSessionDisconnectMessageId"] = 24;
	tls_lua_state["CentreServiceLoginNodeSessionDisconnectIndex"] = 7;
	tls_lua_state["CentreServiceLoginNodeSessionDisconnect"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(7);
	};

	tls_lua_state["CentreServicePlayerServiceMessageId"] = 28;
	tls_lua_state["CentreServicePlayerServiceIndex"] = 8;
	tls_lua_state["CentreServicePlayerService"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(8);
	};

	tls_lua_state["CentreServiceEnterGsSucceedMessageId"] = 38;
	tls_lua_state["CentreServiceEnterGsSucceedIndex"] = 9;
	tls_lua_state["CentreServiceEnterGsSucceed"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(9);
	};

	tls_lua_state["CentreServiceRouteNodeStringMsgMessageId"] = 42;
	tls_lua_state["CentreServiceRouteNodeStringMsgIndex"] = 10;
	tls_lua_state["CentreServiceRouteNodeStringMsg"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(10);
	};

	tls_lua_state["CentreServiceRoutePlayerStringMsgMessageId"] = 45;
	tls_lua_state["CentreServiceRoutePlayerStringMsgIndex"] = 11;
	tls_lua_state["CentreServiceRoutePlayerStringMsg"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(11);
	};

	tls_lua_state["CentreServiceUnRegisterGameNodeMessageId"] = 31;
	tls_lua_state["CentreServiceUnRegisterGameNodeIndex"] = 12;
	tls_lua_state["CentreServiceUnRegisterGameNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return CentreService_Stub::descriptor()->method(12);
	};

}
