#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/common/gate_service.pb.h"


void InitGateServiceLua()
{
	tls_lua_state["GateServiceRegisterGameMessageId"] = 13;
	tls_lua_state["GateServiceRegisterGameIndex"] = 0;
	tls_lua_state["GateServiceRegisterGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(0);
	};

	tls_lua_state["GateServiceUnRegisterGameMessageId"] = 40;
	tls_lua_state["GateServiceUnRegisterGameIndex"] = 1;
	tls_lua_state["GateServiceUnRegisterGame"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(1);
	};

	tls_lua_state["GateServicePlayerEnterGameNodeMessageId"] = 11;
	tls_lua_state["GateServicePlayerEnterGameNodeIndex"] = 2;
	tls_lua_state["GateServicePlayerEnterGameNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(2);
	};

	tls_lua_state["GateServiceSendMessageToPlayerMessageId"] = 63;
	tls_lua_state["GateServiceSendMessageToPlayerIndex"] = 3;
	tls_lua_state["GateServiceSendMessageToPlayer"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(3);
	};

	tls_lua_state["GateServiceKickSessionByCentreMessageId"] = 30;
	tls_lua_state["GateServiceKickSessionByCentreIndex"] = 4;
	tls_lua_state["GateServiceKickSessionByCentre"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(4);
	};

	tls_lua_state["GateServiceRouteNodeMessageMessageId"] = 12;
	tls_lua_state["GateServiceRouteNodeMessageIndex"] = 5;
	tls_lua_state["GateServiceRouteNodeMessage"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(5);
	};

	tls_lua_state["GateServiceRoutePlayerMessageMessageId"] = 15;
	tls_lua_state["GateServiceRoutePlayerMessageIndex"] = 6;
	tls_lua_state["GateServiceRoutePlayerMessage"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(6);
	};

	tls_lua_state["GateServiceBroadcastToPlayersMessageId"] = 14;
	tls_lua_state["GateServiceBroadcastToPlayersIndex"] = 7;
	tls_lua_state["GateServiceBroadcastToPlayers"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GateService_Stub::descriptor()->method(7);
	};

}
