#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "centre_node.h"
#include "node/centre_node_info.h"


void InitGrpcDeployServiceResponseHandler() {
	 AsyncDeployServiceGetNodeInfoHandler = [](const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>& call) {
		gCentreNodeInfo.SetNodeId(call->reply.node_id());
		gCentreNodeInfo.GetNodeInfo().set_lease_id(call->reply.lease_id());
		gCentreNode->StartRpcServer();
		};

}

