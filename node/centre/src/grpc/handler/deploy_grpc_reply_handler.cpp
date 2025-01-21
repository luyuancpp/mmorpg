#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "centre_node.h"
#include "node/centre_node_info.h"

#include "grpc/generator/deploy_service_grpc.h"

void InitGrpcDeploySercieResponseHandler() {
    using Function = std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)>;
    extern Function AsyncDeployServiceGetNodeInfoHandler;
    AsyncDeployServiceGetNodeInfoHandler = [](const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>& call) {
        gCentreNodeInfo.SetNodeId(call->reply.node_id());
        gCentreNodeInfo.GetNodeInfo().set_lease_id(call->reply.lease_id());
        gCentreNode->StartRpcServer(call->reply.info());
        };
}
