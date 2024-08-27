#include <cstdint>

#include "thread_local/storage_lua.h"
#include "common/deploy_service.pb.h"


void InitDeployServiceLua()
{
	tls_lua_state["DeployServiceGetNodeInfoMessageId"] = 10;
	tls_lua_state["DeployServiceGetNodeInfoIndex"] = 0;
	tls_lua_state["DeployServiceGetNodeInfo"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return DeployService_Stub::descriptor()->method(0);
	};

}
