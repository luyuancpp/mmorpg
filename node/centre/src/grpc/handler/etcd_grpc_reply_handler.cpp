#include <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include <grpcpp/create_channel.h>
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "centre_node.h"
#include "node/centre_node_info.h"
#include "node/system/node_system.h"
#include "network/network_constants.h"
#include "util/network_utils.h"


void InitGrpcetcdserverpbResponseHandler() {
	AsyncetcdserverpbKVRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {
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

	AsyncetcdserverpbWatchWatchHandler = [](const ::etcdserverpb::WatchResponse& response) {

		if (response.created()) {
			LOG_INFO << "Watch created: " << response.created();
			return;
		}

		if (response.canceled()) {
			LOG_INFO << "Watch canceled. Reason: " << response.cancel_reason();
			if (response.compact_revision() > 0) {
				LOG_ERROR << "Revision " << response.compact_revision() << " has been compacted.\n";
				// 需要重新发起 watch 请求，从最新 revision 开始
			}
			return;
		}

		for (const auto& event : response.events()) {
			if (event.type() == mvccpb::Event_EventType::Event_EventType_PUT) {
				LOG_INFO << "Key put: " << event.kv().key() << "\n";
			}
			else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
				LOG_INFO << "Key deleted: " << event.kv().key() << "\n";
			}
		}
	};
}
