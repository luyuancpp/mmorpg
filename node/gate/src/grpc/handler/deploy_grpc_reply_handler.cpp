#include <functional>
#include <memory>

#include "gate_node.h"
#include "grpc/generator/deploy_service_grpc.h"

void InitGrpcDeployServiceResponseHandler() {
    AsyncDeployServiceGetNodeInfoHandlerFunctionType AsyncDeployServiceGetNodeInfoHandler = [](const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>& call) {
        gGateNode->GetNodeInfo().set_lease_id(call->reply.lease_id());
        gGateNode->GetNodeInfo().set_node_id(call->reply.node_id());
        gGateNode->StartRpcServer(call->reply.info());
        };
}