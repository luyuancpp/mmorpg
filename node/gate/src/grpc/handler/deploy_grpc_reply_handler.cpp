#include <functional>
#include <memory>

#include "gate_node.h"
#include "grpc/generator/deploy_service_grpc.h"

void InitGrpcDeployServiceResponseHandler() {
    using Function = std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)>;
    extern Function AsyncDeployServiceGetNodeInfoHandler;
    AsyncDeployServiceGetNodeInfoHandler = [](const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>& call) {
        g_gate_node->GetNodeInfo().set_lease_id(call->reply.lease_id());
        g_gate_node->GetNodeInfo().set_node_id(call->reply.node_id());
        g_gate_node->StartRpcServer(call->reply.info());
        };
}
