#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/logic/client_player/player_common.pb.h"


void InitPlayerClientCommonServiceLua()
{
	tls_lua_state["PlayerClientCommonServiceSendTipToClientMessageId"] = 0;
	tls_lua_state["PlayerClientCommonServiceSendTipToClientIndex"] = 0;
	tls_lua_state["PlayerClientCommonServiceSendTipToClient"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return PlayerClientCommonService_Stub::descriptor()->method(0);
	};

	tls_lua_state["PlayerClientCommonServiceKickPlayerMessageId"] = 37;
	tls_lua_state["PlayerClientCommonServiceKickPlayerIndex"] = 1;
	tls_lua_state["PlayerClientCommonServiceKickPlayer"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return PlayerClientCommonService_Stub::descriptor()->method(1);
	};

}
