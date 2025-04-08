#include "muduo/base/Logging.h"

#include <functional>
#include <memory>

#include "scene_node.h"
#include "node/scene_node_info.h"

#include "grpc/generator/deploy_service_grpc.h"


void InitGrpcDeployServiceResponseHandler() {
	AsyncDeployServiceGetNodeInfoHandlerFunctionType AsyncDeployServiceGetNodeInfoHandler = [](const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>& call) {
		gSceneNodeInfo.SetNodeId(call->reply.node_id());
		gSceneNodeInfo.GetNodeInfo().set_lease_id(call->reply.lease_id());
		gSceneNode->StartRpcServer();
		};
}