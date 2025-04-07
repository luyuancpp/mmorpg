#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "grpc/generator/etcd_grpc.h"
#include "node/system/node_system.h"
#include "network/network_constants.h"
#include "util/network_utils.h"
#include "scene_node.h"

void InitGrpcetcdserverpbKVResponseHandler() {
	AsyncetcdserverpbKVRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {
			for (const auto& kv : call->reply.kvs()) {
				if (kv.value() == FormatIpAndPort())
				{
					continue;
				}

				if (NodeSystem::GetServiceTypeFromPrefix(kv.key()) == kDeploy) {

					// 定时更新节点租约
					gSceneNode->InitializeDeployService(kv.value());

					// 处理部署服务的键值对
					LOG_INFO << "Deploy Service Key: " << kv.key() << ", Value: " << kv.value();
				}
				else if (NodeSystem::GetServiceTypeFromPrefix(kv.key()) == kSceneNode) {
					// 处理场景节点的键值对
					LOG_INFO << "Scene Node Key: " << kv.key() << ", Value: " << kv.value();
				}
				else if (NodeSystem::GetServiceTypeFromPrefix(kv.key()) == kGateNode) {
					// 处理网关节点的键值对
					LOG_INFO << "Gate Node Key: " << kv.key() << ", Value: " << kv.value();
				}
				else if (NodeSystem::GetServiceTypeFromPrefix(kv.key()) == kCentreNode) {
					// 处理中心节点的键值对
					LOG_INFO << "Centre Node Key: " << kv.key() << ", Value: " << kv.value();
				}
				else {
					LOG_ERROR << "Unknown service type for key: " << kv.key();
				}
			}
		}
		else {
			LOG_ERROR << "RPC failed: " << call->status.error_message();
		}
		};

	AsyncetcdserverpbKVPutHandler = [](const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>& call) {
		};

	AsyncetcdserverpbKVDeleteRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>& call) {
		};

	AsyncetcdserverpbKVTxnHandler = [](const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>& call) {
		};
}

