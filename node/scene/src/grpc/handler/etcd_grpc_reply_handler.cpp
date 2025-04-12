#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "node/system/node_system.h"
#include "network/network_constants.h"
#include "util/network_utils.h"
#include "scene_node.h"

void InitGrpcetcdserverpbKVResponseHandler() {
	AsyncetcdserverpbKVRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {
			for (const auto& kv : call->reply.kvs()) {
				gSceneNode->HandleServiceNode(kv.key(), kv.value());
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

