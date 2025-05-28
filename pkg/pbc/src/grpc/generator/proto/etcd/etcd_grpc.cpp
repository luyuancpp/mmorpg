#include "muduo/base/Logging.h"
#include "etcd_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

namespace etcdserverpb{
#pragma region KVRange
struct KVRangeCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVRangeGrpcClientCall>&)>;
AsyncKVRangeHandlerFunctionType AsyncKVRangeHandler;

void AsyncCompleteGrpcKVRange(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncKVRangeGrpcClientCall> call(
        static_cast<AsyncKVRangeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVRangeHandler) {
            AsyncKVRangeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request) {

    AsyncKVRangeGrpcClientCall* call = new AsyncKVRangeGrpcClientCall;
    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncRange(&call->context, request,
                                  &registry.get<KVRangeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVRangeGrpcClientCall* call = new AsyncKVRangeGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncRange(&call->context, request,
                                  &registry.get<KVRangeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region KVPut
struct KVPutCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVPutGrpcClientCall>&)>;
AsyncKVPutHandlerFunctionType AsyncKVPutHandler;

void AsyncCompleteGrpcKVPut(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncKVPutGrpcClientCall> call(
        static_cast<AsyncKVPutGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVPutHandler) {
            AsyncKVPutHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request) {

    AsyncKVPutGrpcClientCall* call = new AsyncKVPutGrpcClientCall;
    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncPut(&call->context, request,
                                  &registry.get<KVPutCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVPutGrpcClientCall* call = new AsyncKVPutGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncPut(&call->context, request,
                                  &registry.get<KVPutCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region KVDeleteRange
struct KVDeleteRangeCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVDeleteRangeGrpcClientCall>&)>;
AsyncKVDeleteRangeHandlerFunctionType AsyncKVDeleteRangeHandler;

void AsyncCompleteGrpcKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncKVDeleteRangeGrpcClientCall> call(
        static_cast<AsyncKVDeleteRangeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVDeleteRangeHandler) {
            AsyncKVDeleteRangeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request) {

    AsyncKVDeleteRangeGrpcClientCall* call = new AsyncKVDeleteRangeGrpcClientCall;
    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncDeleteRange(&call->context, request,
                                  &registry.get<KVDeleteRangeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVDeleteRangeGrpcClientCall* call = new AsyncKVDeleteRangeGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncDeleteRange(&call->context, request,
                                  &registry.get<KVDeleteRangeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region KVTxn
struct KVTxnCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVTxnGrpcClientCall>&)>;
AsyncKVTxnHandlerFunctionType AsyncKVTxnHandler;

void AsyncCompleteGrpcKVTxn(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncKVTxnGrpcClientCall> call(
        static_cast<AsyncKVTxnGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVTxnHandler) {
            AsyncKVTxnHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request) {

    AsyncKVTxnGrpcClientCall* call = new AsyncKVTxnGrpcClientCall;
    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncTxn(&call->context, request,
                                  &registry.get<KVTxnCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVTxnGrpcClientCall* call = new AsyncKVTxnGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncTxn(&call->context, request,
                                  &registry.get<KVTxnCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region KVCompact
struct KVCompactCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVCompactGrpcClientCall>&)>;
AsyncKVCompactHandlerFunctionType AsyncKVCompactHandler;

void AsyncCompleteGrpcKVCompact(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncKVCompactGrpcClientCall> call(
        static_cast<AsyncKVCompactGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVCompactHandler) {
            AsyncKVCompactHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request) {

    AsyncKVCompactGrpcClientCall* call = new AsyncKVCompactGrpcClientCall;
    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncCompact(&call->context, request,
                                  &registry.get<KVCompactCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVCompactGrpcClientCall* call = new AsyncKVCompactGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncCompact(&call->context, request,
                                  &registry.get<KVCompactCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region WatchWatch
struct WatchWatchCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncWatchWatchHandlerFunctionType = std::function<void(const ::etcdserverpb::WatchResponse&)>;
AsyncWatchWatchHandlerFunctionType AsyncWatchWatchHandler;
void TryWriteNextNextWatchWatch(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    auto& writeInProgress = registry.get<WatchRequestWriteInProgress>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;

    if (writeInProgress.isInProgress || pendingWritesBuffer.empty()) {
        return;
    }

    auto& client = registry.get<AsyncWatchWatchGrpcClient>(nodeEntity);
    auto& request = pendingWritesBuffer.front();

    writeInProgress.isInProgress = true;
    client.stream->Write(request, (void*)(GrpcOperation::WRITE));
}

void AsyncCompleteGrpcWatchWatch(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    auto& client = registry.get<AsyncWatchWatchGrpcClient>(nodeEntity);
    auto& writeInProgress = registry.get<WatchRequestWriteInProgress>(nodeEntity);

    switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))) {
        case GrpcOperation::WRITE: {
            auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
            if (!pendingWritesBuffer.empty()) {
                pendingWritesBuffer.pop_front();
            }
            writeInProgress.isInProgress = false;
            TryWriteNextNextWatchWatch(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::WRITES_DONE:
            client.stream->Finish(&client.status, (void*)(GrpcOperation::FINISH));
            break;
        case GrpcOperation::FINISH:
            cq.Shutdown();
            break;
        case GrpcOperation::READ: {
            auto& response = registry.get<::etcdserverpb::WatchResponse>(nodeEntity);
            if (AsyncWatchWatchHandler) {
                AsyncWatchWatchHandler(response);
            }
            client.stream->Read(&response, (void*)GrpcOperation::READ);
            TryWriteNextNextWatchWatch(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::INIT: {
            auto& response = registry.get<::etcdserverpb::WatchResponse>(nodeEntity);
            client.stream->Read(&response, (void*)GrpcOperation::READ);
            TryWriteNextNextWatchWatch(registry, nodeEntity, cq);
            break;
        }
        default:
            break;
    }
}


void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request) {

    auto& cq = registry.get<WatchWatchCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextWatchWatch(registry, nodeEntity, cq);

}

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto& cq = registry.get<WatchWatchCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextWatchWatch(registry, nodeEntity, cq);

}
#pragma endregion



#pragma region LeaseLeaseGrant
struct LeaseLeaseGrantCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLeaseLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseGrantGrpcClientCall>&)>;
AsyncLeaseLeaseGrantHandlerFunctionType AsyncLeaseLeaseGrantHandler;

void AsyncCompleteGrpcLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncLeaseLeaseGrantGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseGrantGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseGrantHandler) {
            AsyncLeaseLeaseGrantHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request) {

    AsyncLeaseLeaseGrantGrpcClientCall* call = new AsyncLeaseLeaseGrantGrpcClientCall;
    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseGrant(&call->context, request,
                                  &registry.get<LeaseLeaseGrantCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseGrantGrpcClientCall* call = new AsyncLeaseLeaseGrantGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseGrant(&call->context, request,
                                  &registry.get<LeaseLeaseGrantCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region LeaseLeaseRevoke
struct LeaseLeaseRevokeCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLeaseLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseRevokeGrpcClientCall>&)>;
AsyncLeaseLeaseRevokeHandlerFunctionType AsyncLeaseLeaseRevokeHandler;

void AsyncCompleteGrpcLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncLeaseLeaseRevokeGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseRevokeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseRevokeHandler) {
            AsyncLeaseLeaseRevokeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request) {

    AsyncLeaseLeaseRevokeGrpcClientCall* call = new AsyncLeaseLeaseRevokeGrpcClientCall;
    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseRevoke(&call->context, request,
                                  &registry.get<LeaseLeaseRevokeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseRevokeGrpcClientCall* call = new AsyncLeaseLeaseRevokeGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseRevoke(&call->context, request,
                                  &registry.get<LeaseLeaseRevokeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region LeaseLeaseKeepAlive
struct LeaseLeaseKeepAliveCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const ::etcdserverpb::LeaseKeepAliveResponse&)>;
AsyncLeaseLeaseKeepAliveHandlerFunctionType AsyncLeaseLeaseKeepAliveHandler;
void TryWriteNextNextLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    auto& writeInProgress = registry.get<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;

    if (writeInProgress.isInProgress || pendingWritesBuffer.empty()) {
        return;
    }

    auto& client = registry.get<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
    auto& request = pendingWritesBuffer.front();

    writeInProgress.isInProgress = true;
    client.stream->Write(request, (void*)(GrpcOperation::WRITE));
}

void AsyncCompleteGrpcLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    auto& client = registry.get<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
    auto& writeInProgress = registry.get<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);

    switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))) {
        case GrpcOperation::WRITE: {
            auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
            if (!pendingWritesBuffer.empty()) {
                pendingWritesBuffer.pop_front();
            }
            writeInProgress.isInProgress = false;
            TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::WRITES_DONE:
            client.stream->Finish(&client.status, (void*)(GrpcOperation::FINISH));
            break;
        case GrpcOperation::FINISH:
            cq.Shutdown();
            break;
        case GrpcOperation::READ: {
            auto& response = registry.get<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);
            if (AsyncLeaseLeaseKeepAliveHandler) {
                AsyncLeaseLeaseKeepAliveHandler(response);
            }
            client.stream->Read(&response, (void*)GrpcOperation::READ);
            TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::INIT: {
            auto& response = registry.get<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);
            client.stream->Read(&response, (void*)GrpcOperation::READ);
            TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);
            break;
        }
        default:
            break;
    }
}


void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request) {

    auto& cq = registry.get<LeaseLeaseKeepAliveCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);

}

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto& cq = registry.get<LeaseLeaseKeepAliveCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);

}
#pragma endregion



#pragma region LeaseLeaseTimeToLive
struct LeaseLeaseTimeToLiveCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseTimeToLiveGrpcClientCall>&)>;
AsyncLeaseLeaseTimeToLiveHandlerFunctionType AsyncLeaseLeaseTimeToLiveHandler;

void AsyncCompleteGrpcLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncLeaseLeaseTimeToLiveGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseTimeToLiveGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseTimeToLiveHandler) {
            AsyncLeaseLeaseTimeToLiveHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request) {

    AsyncLeaseLeaseTimeToLiveGrpcClientCall* call = new AsyncLeaseLeaseTimeToLiveGrpcClientCall;
    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseTimeToLive(&call->context, request,
                                  &registry.get<LeaseLeaseTimeToLiveCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseTimeToLiveGrpcClientCall* call = new AsyncLeaseLeaseTimeToLiveGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseTimeToLive(&call->context, request,
                                  &registry.get<LeaseLeaseTimeToLiveCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region LeaseLeaseLeases
struct LeaseLeaseLeasesCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLeaseLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseLeasesGrpcClientCall>&)>;
AsyncLeaseLeaseLeasesHandlerFunctionType AsyncLeaseLeaseLeasesHandler;

void AsyncCompleteGrpcLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncLeaseLeaseLeasesGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseLeasesGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseLeasesHandler) {
            AsyncLeaseLeaseLeasesHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request) {

    AsyncLeaseLeaseLeasesGrpcClientCall* call = new AsyncLeaseLeaseLeasesGrpcClientCall;
    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseLeases(&call->context, request,
                                  &registry.get<LeaseLeaseLeasesCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseLeasesGrpcClientCall* call = new AsyncLeaseLeaseLeasesGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseLeases(&call->context, request,
                                  &registry.get<LeaseLeaseLeasesCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



void InitKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<KVRangeCompleteQueue>(nodeEntity);

    registry.emplace<KVPutCompleteQueue>(nodeEntity);

    registry.emplace<KVDeleteRangeCompleteQueue>(nodeEntity);

    registry.emplace<KVTxnCompleteQueue>(nodeEntity);

    registry.emplace<KVCompactCompleteQueue>(nodeEntity);
}
void InitWatchCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<WatchWatchCompleteQueue>(nodeEntity);

    {
        auto& client = registry.emplace<AsyncWatchWatchGrpcClient>(nodeEntity);
        registry.emplace<WatchRequestBuffer>(nodeEntity);
        registry.emplace<WatchRequestWriteInProgress>(nodeEntity);
        registry.emplace<::etcdserverpb::WatchResponse>(nodeEntity);
        registry.emplace<::etcdserverpb::WatchRequest>(nodeEntity);

        client.stream = registry
            .get<WatchStubPtr>(nodeEntity)
            ->AsyncWatch(&client.context,
                               &registry.get<WatchWatchCompleteQueue>(nodeEntity).cq,
                               (void*)(GrpcOperation::INIT));
    }
}
void InitLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<LeaseLeaseGrantCompleteQueue>(nodeEntity);

    registry.emplace<LeaseLeaseRevokeCompleteQueue>(nodeEntity);

    registry.emplace<LeaseLeaseKeepAliveCompleteQueue>(nodeEntity);

    {
        auto& client = registry.emplace<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
        registry.emplace<LeaseKeepAliveRequestBuffer>(nodeEntity);
        registry.emplace<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);
        registry.emplace<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);
        registry.emplace<::etcdserverpb::LeaseKeepAliveRequest>(nodeEntity);

        client.stream = registry
            .get<LeaseStubPtr>(nodeEntity)
            ->AsyncLeaseKeepAlive(&client.context,
                               &registry.get<LeaseLeaseKeepAliveCompleteQueue>(nodeEntity).cq,
                               (void*)(GrpcOperation::INIT));
    }

    registry.emplace<LeaseLeaseTimeToLiveCompleteQueue>(nodeEntity);

    registry.emplace<LeaseLeaseLeasesCompleteQueue>(nodeEntity);
}
void HandleKVCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<KVRangeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcKVRange(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<KVPutCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcKVPut(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<KVDeleteRangeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcKVDeleteRange(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<KVTxnCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcKVTxn(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<KVCompactCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcKVCompact(registry, e, completeQueueComp.cq);
        }
    }}
void HandleWatchCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<WatchWatchCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcWatchWatch(registry, e, completeQueueComp.cq);
        }
    }}
void HandleLeaseCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<LeaseLeaseGrantCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLeaseLeaseGrant(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LeaseLeaseRevokeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLeaseLeaseRevoke(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LeaseLeaseKeepAliveCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLeaseLeaseKeepAlive(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LeaseLeaseTimeToLiveCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLeaseLeaseTimeToLive(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LeaseLeaseLeasesCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLeaseLeaseLeases(registry, e, completeQueueComp.cq);
        }
    }}}// namespace etcdserverpb
