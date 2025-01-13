#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/common/deploy_service.pb.h"


void InitDeployServiceLua()
{
	tls_lua_state["DeployServiceGetNodeInfoMessageId"] = 10;
	tls_lua_state["DeployServiceGetNodeInfoIndex"] = 0;
	tls_lua_state["DeployServiceGetNodeInfo"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return DeployService_Stub::descriptor()->method(0);
	};

	tls_lua_state["DeployServiceGetIDMessageId"] = 72;
	tls_lua_state["DeployServiceGetIDIndex"] = 1;
	tls_lua_state["DeployServiceGetID"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return DeployService_Stub::descriptor()->method(1);
	};

	tls_lua_state["DeployServiceReleaseIDMessageId"] = 73;
	tls_lua_state["DeployServiceReleaseIDIndex"] = 2;
	tls_lua_state["DeployServiceReleaseID"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return DeployService_Stub::descriptor()->method(2);
	};

}
