#pragma once
#include "deploy_node.pb.h"

#include "src/network/rpc_stub.h"

using DeployStub = RpcStub<deploy::DeployService_Stub>;




