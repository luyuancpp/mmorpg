#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include <grpcpp/create_channel.h>
#include "grpc/generator/etcd_grpc.h"
#include "centre_node.h"
#include "node/centre_node_info.h"
#include "node/system/node_system.h"
#include "network/network_constants.h"
#include "util/network_utils.h"


void InitGrpcetcdserverpbKVResponseHandler() {
	AsyncetcdserverpbKVRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {

			LOG_INFO << call->reply.DebugString();

			for (const auto& kv : call->reply.kvs()) {
				gCentreNode->HandleServiceNode(kv.key(), kv.value());
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
