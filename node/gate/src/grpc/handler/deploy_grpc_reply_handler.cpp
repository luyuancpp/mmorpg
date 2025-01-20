#include <functional>
#include <memory>

#include "gate_node.h"
#include "grpc/generator/deploy_service_grpc.h"

extern std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)> AsyncDeployServiceGetNodeInfoHandler;


void InitCallBack() {
    AsyncDeployServiceGetNodeInfoHandler = [](const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>& call) {
        g_gate_node->GetNodeInfo().set_lease_id(call->reply.lease_id());
        g_gate_node->SetNodeId(call->reply.node_id());
        g_gate_node->StartServer(call->reply.info());
        };
}
