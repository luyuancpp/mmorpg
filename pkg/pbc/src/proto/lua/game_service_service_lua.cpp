#include <cstdint>

#include "thread_local/storage_lua.h"
#include "common/game_service.pb.h"


void InitGameServiceLua()
{
	tls_lua_state["GameServicePlayerEnterGameNodeMessageId"] = 7;
	tls_lua_state["GameServicePlayerEnterGameNodeIndex"] = 0;
	tls_lua_state["GameServicePlayerEnterGameNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(0);
	};

	tls_lua_state["GameServiceSendMessageToPlayerMessageId"] = 35;
	tls_lua_state["GameServiceSendMessageToPlayerIndex"] = 1;
	tls_lua_state["GameServiceSendMessageToPlayer"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(1);
	};

	tls_lua_state["GameServiceClientSendMessageToPlayerMessageId"] = 26;
	tls_lua_state["GameServiceClientSendMessageToPlayerIndex"] = 2;
	tls_lua_state["GameServiceClientSendMessageToPlayer"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(2);
	};

	tls_lua_state["GameServiceSessionDisconnectMessageId"] = 57;
	tls_lua_state["GameServiceSessionDisconnectIndex"] = 3;
	tls_lua_state["GameServiceSessionDisconnect"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(3);
	};

	tls_lua_state["GameServiceRegisterGateNodeMessageId"] = 61;
	tls_lua_state["GameServiceRegisterGateNodeIndex"] = 4;
	tls_lua_state["GameServiceRegisterGateNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(4);
	};

	tls_lua_state["GameServiceCentreSendToPlayerViaGameNodeMessageId"] = 25;
	tls_lua_state["GameServiceCentreSendToPlayerViaGameNodeIndex"] = 5;
	tls_lua_state["GameServiceCentreSendToPlayerViaGameNode"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(5);
	};

	tls_lua_state["GameServiceInvokePlayerServiceMessageId"] = 59;
	tls_lua_state["GameServiceInvokePlayerServiceIndex"] = 6;
	tls_lua_state["GameServiceInvokePlayerService"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(6);
	};

	tls_lua_state["GameServiceRouteNodeStringMsgMessageId"] = 64;
	tls_lua_state["GameServiceRouteNodeStringMsgIndex"] = 7;
	tls_lua_state["GameServiceRouteNodeStringMsg"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(7);
	};

	tls_lua_state["GameServiceRoutePlayerStringMsgMessageId"] = 65;
	tls_lua_state["GameServiceRoutePlayerStringMsgIndex"] = 8;
	tls_lua_state["GameServiceRoutePlayerStringMsg"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(8);
	};

	tls_lua_state["GameServiceUpdateSessionDetailMessageId"] = 39;
	tls_lua_state["GameServiceUpdateSessionDetailIndex"] = 9;
	tls_lua_state["GameServiceUpdateSessionDetail"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(9);
	};

	tls_lua_state["GameServiceEnterSceneMessageId"] = 75;
	tls_lua_state["GameServiceEnterSceneIndex"] = 10;
	tls_lua_state["GameServiceEnterScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(10);
	};

	tls_lua_state["GameServiceCreateSceneMessageId"] = 44;
	tls_lua_state["GameServiceCreateSceneIndex"] = 11;
	tls_lua_state["GameServiceCreateScene"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return GameService_Stub::descriptor()->method(11);
	};

}
