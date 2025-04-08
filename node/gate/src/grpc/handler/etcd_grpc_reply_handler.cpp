#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "grpc/generator/etcd_grpc.h"
#include "node/system/node_system.h"
#include "network/network_constants.h"
#include "util/network_utils.h"
#include "gate_node.h"
#include "node/comp/node_comp.h"

void InitGrpcetcdserverpbKVResponseHandler() {
	AsyncetcdserverpbKVRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {

			for (const auto& kv : call->reply.kvs()) {
				if (kv.value() == gGateNode->FormatIpAndPort())
				{
					continue;
				}

				auto serviceNodeType = NodeSystem::GetServiceTypeFromPrefix(kv.key());

				if (serviceNodeType == kDeploy) {

					// 定时更新节点租约
					gGateNode->InitializeDeployService(kv.value());

					// 处理部署服务的键值对
					LOG_INFO << "Deploy Service Key: " << kv.key() << ", Value: " << kv.value();
				}
				else if (eNodeType_IsValid(serviceNodeType)) {
					
					gGateNode->ParseJsonToServiceNode(kv.value(), serviceNodeType);

					LOG_INFO << "Service Node Key: " << kv.key() << ", Value: " << kv.value();
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

