#include "muduo/base/Logging.h"
#include "etcd_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"



namespace etcdserverpb{
struct EtcdCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region KVRange

using AsyncKVRangeHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::RangeResponse&)>;
AsyncKVRangeHandlerFunctionType AsyncKVRangeHandler;



void AsyncCompleteGrpcKVRange(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncKVRangeGrpcClientCall> call(
        static_cast<AsyncKVRangeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVRangeHandler) {
            AsyncKVRangeHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVRangeGrpcClientCall* call = new AsyncKVRangeGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncRange(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::RangeRequest& derived = static_cast<const ::etcdserverpb::RangeRequest&>(message);
	SendKVRange(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region KVPut

using AsyncKVPutHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::PutResponse&)>;
AsyncKVPutHandlerFunctionType AsyncKVPutHandler;



void AsyncCompleteGrpcKVPut(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncKVPutGrpcClientCall> call(
        static_cast<AsyncKVPutGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVPutHandler) {
            AsyncKVPutHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVPutGrpcClientCall* call = new AsyncKVPutGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncPut(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::PutRequest& derived = static_cast<const ::etcdserverpb::PutRequest&>(message);
	SendKVPut(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region KVDeleteRange

using AsyncKVDeleteRangeHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::DeleteRangeResponse&)>;
AsyncKVDeleteRangeHandlerFunctionType AsyncKVDeleteRangeHandler;



void AsyncCompleteGrpcKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncKVDeleteRangeGrpcClientCall> call(
        static_cast<AsyncKVDeleteRangeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVDeleteRangeHandler) {
            AsyncKVDeleteRangeHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVDeleteRangeGrpcClientCall* call = new AsyncKVDeleteRangeGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncDeleteRange(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::DeleteRangeRequest& derived = static_cast<const ::etcdserverpb::DeleteRangeRequest&>(message);
	SendKVDeleteRange(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region KVTxn

using AsyncKVTxnHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::TxnResponse&)>;
AsyncKVTxnHandlerFunctionType AsyncKVTxnHandler;



void AsyncCompleteGrpcKVTxn(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncKVTxnGrpcClientCall> call(
        static_cast<AsyncKVTxnGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVTxnHandler) {
            AsyncKVTxnHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVTxnGrpcClientCall* call = new AsyncKVTxnGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncTxn(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::TxnRequest& derived = static_cast<const ::etcdserverpb::TxnRequest&>(message);
	SendKVTxn(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region KVCompact

using AsyncKVCompactHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::CompactionResponse&)>;
AsyncKVCompactHandlerFunctionType AsyncKVCompactHandler;



void AsyncCompleteGrpcKVCompact(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncKVCompactGrpcClientCall> call(
        static_cast<AsyncKVCompactGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncKVCompactHandler) {
            AsyncKVCompactHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncKVCompactGrpcClientCall* call = new AsyncKVCompactGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<KVStubPtr>(nodeEntity)
        ->PrepareAsyncCompact(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::CompactionRequest& derived = static_cast<const ::etcdserverpb::CompactionRequest&>(message);
	SendKVCompact(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region WatchWatch

using AsyncWatchWatchHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::WatchResponse&)>;
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

void AsyncCompleteGrpcWatchWatch(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
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
                AsyncWatchWatchHandler(client.context, response);
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

    auto& cq = registry.get< EtcdCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextWatchWatch(registry, nodeEntity, cq);

}

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto& cq = registry.get< EtcdCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextWatchWatch(registry, nodeEntity, cq);

}

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::WatchRequest& derived = static_cast<const ::etcdserverpb::WatchRequest&>(message);
	SendWatchWatch(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LeaseLeaseGrant

using AsyncLeaseLeaseGrantHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::LeaseGrantResponse&)>;
AsyncLeaseLeaseGrantHandlerFunctionType AsyncLeaseLeaseGrantHandler;



void AsyncCompleteGrpcLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncLeaseLeaseGrantGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseGrantGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseGrantHandler) {
            AsyncLeaseLeaseGrantHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseGrantGrpcClientCall* call = new AsyncLeaseLeaseGrantGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseGrant(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::LeaseGrantRequest& derived = static_cast<const ::etcdserverpb::LeaseGrantRequest&>(message);
	SendLeaseLeaseGrant(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LeaseLeaseRevoke

using AsyncLeaseLeaseRevokeHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::LeaseRevokeResponse&)>;
AsyncLeaseLeaseRevokeHandlerFunctionType AsyncLeaseLeaseRevokeHandler;



void AsyncCompleteGrpcLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncLeaseLeaseRevokeGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseRevokeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseRevokeHandler) {
            AsyncLeaseLeaseRevokeHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseRevokeGrpcClientCall* call = new AsyncLeaseLeaseRevokeGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseRevoke(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::LeaseRevokeRequest& derived = static_cast<const ::etcdserverpb::LeaseRevokeRequest&>(message);
	SendLeaseLeaseRevoke(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LeaseLeaseKeepAlive

using AsyncLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::LeaseKeepAliveResponse&)>;
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

void AsyncCompleteGrpcLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
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
                AsyncLeaseLeaseKeepAliveHandler(client.context, response);
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

    auto& cq = registry.get< EtcdCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);

}

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto& cq = registry.get< EtcdCompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextLeaseLeaseKeepAlive(registry, nodeEntity, cq);

}

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::LeaseKeepAliveRequest& derived = static_cast<const ::etcdserverpb::LeaseKeepAliveRequest&>(message);
	SendLeaseLeaseKeepAlive(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LeaseLeaseTimeToLive

using AsyncLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::LeaseTimeToLiveResponse&)>;
AsyncLeaseLeaseTimeToLiveHandlerFunctionType AsyncLeaseLeaseTimeToLiveHandler;



void AsyncCompleteGrpcLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncLeaseLeaseTimeToLiveGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseTimeToLiveGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseTimeToLiveHandler) {
            AsyncLeaseLeaseTimeToLiveHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseTimeToLiveGrpcClientCall* call = new AsyncLeaseLeaseTimeToLiveGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseTimeToLive(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::LeaseTimeToLiveRequest& derived = static_cast<const ::etcdserverpb::LeaseTimeToLiveRequest&>(message);
	SendLeaseLeaseTimeToLive(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LeaseLeaseLeases

using AsyncLeaseLeaseLeasesHandlerFunctionType = std::function<void(const ClientContext&, const ::etcdserverpb::LeaseLeasesResponse&)>;
AsyncLeaseLeaseLeasesHandlerFunctionType AsyncLeaseLeaseLeasesHandler;



void AsyncCompleteGrpcLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    std::unique_ptr<AsyncLeaseLeaseLeasesGrpcClientCall> call(
        static_cast<AsyncLeaseLeaseLeasesGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLeaseLeaseLeasesHandler) {
            AsyncLeaseLeaseLeasesHandler(call->context, call->reply);
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
                                  &registry.get< EtcdCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLeaseLeaseLeasesGrpcClientCall* call = new AsyncLeaseLeaseLeasesGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LeaseStubPtr>(nodeEntity)
        ->PrepareAsyncLeaseLeases(&call->context, request,
                                  &registry.get< EtcdCompleteQueue >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::etcdserverpb::LeaseLeasesRequest& derived = static_cast<const ::etcdserverpb::LeaseLeasesRequest&>(message);
	SendLeaseLeaseLeases(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion





void InitEtcdCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace< EtcdCompleteQueue >(nodeEntity);






    {
        auto& client = registry.emplace<AsyncWatchWatchGrpcClient>(nodeEntity);
        registry.emplace<WatchRequestBuffer>(nodeEntity);
        registry.emplace<WatchRequestWriteInProgress>(nodeEntity);
        registry.emplace<::etcdserverpb::WatchResponse>(nodeEntity);
        registry.emplace<::etcdserverpb::WatchRequest>(nodeEntity);

        client.stream = registry
            .get<WatchStubPtr>(nodeEntity)
            ->AsyncWatch(&client.context,
                               &registry.get< EtcdCompleteQueue>(nodeEntity).cq,
                               (void*)(GrpcOperation::INIT));
    }




    {
        auto& client = registry.emplace<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
        registry.emplace<LeaseKeepAliveRequestBuffer>(nodeEntity);
        registry.emplace<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);
        registry.emplace<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);
        registry.emplace<::etcdserverpb::LeaseKeepAliveRequest>(nodeEntity);

        client.stream = registry
            .get<LeaseStubPtr>(nodeEntity)
            ->AsyncLeaseKeepAlive(&client.context,
                               &registry.get< EtcdCompleteQueue>(nodeEntity).cq,
                               (void*)(GrpcOperation::INIT));
    }


}


void HandleEtcdCompletedQueueMessage(entt::registry& registry) {
  
	auto&& view = registry.view< EtcdCompleteQueue>();
    for (auto&& [e, completeQueueComp] : view.each()) {
		void* got_tag = nullptr;
		bool ok = false;
		gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
		if (grpc::CompletionQueue::GOT_EVENT != completeQueueComp.cq.AsyncNext(&got_tag, &ok, tm)) {
			return;
		}
		if (!ok) {
			LOG_ERROR << "RPC failed";
			return;
		}
		GrpcMethod type = *reinterpret_cast<GrpcMethod*>(got_tag);
		switch(type){
		{
			case GrpcMethod::KV_Range:
			AsyncCompleteGrpcKVRange(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::KV_Put:
			AsyncCompleteGrpcKVPut(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::KV_DeleteRange:
			AsyncCompleteGrpcKVDeleteRange(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::KV_Txn:
			AsyncCompleteGrpcKVTxn(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::KV_Compact:
			AsyncCompleteGrpcKVCompact(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::Watch_Watch:
			AsyncCompleteGrpcWatchWatch(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::Lease_LeaseGrant:
			AsyncCompleteGrpcLeaseLeaseGrant(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::Lease_LeaseRevoke:
			AsyncCompleteGrpcLeaseLeaseRevoke(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::Lease_LeaseKeepAlive:
			AsyncCompleteGrpcLeaseLeaseKeepAlive(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::Lease_LeaseTimeToLive:
			AsyncCompleteGrpcLeaseLeaseTimeToLive(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		{
			case GrpcMethod::Lease_LeaseLeases:
			AsyncCompleteGrpcLeaseLeaseLeases(registry, e, completeQueueComp.cq, got_tag);
		}
		break;
		default:
			break;
		}
    }
}


void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){
   AsyncKVRangeHandler = handler;
   AsyncKVPutHandler = handler;
   AsyncKVDeleteRangeHandler = handler;
   AsyncKVTxnHandler = handler;
   AsyncKVCompactHandler = handler;
   AsyncWatchWatchHandler = handler;
   AsyncLeaseLeaseGrantHandler = handler;
   AsyncLeaseLeaseRevokeHandler = handler;
   AsyncLeaseLeaseKeepAliveHandler = handler;
   AsyncLeaseLeaseTimeToLiveHandler = handler;
   AsyncLeaseLeaseLeasesHandler = handler;
}


void SetEtcdIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){
	if (!AsyncKVRangeHandler){
   		AsyncKVRangeHandler = handler;
	}
	if (!AsyncKVPutHandler){
   		AsyncKVPutHandler = handler;
	}
	if (!AsyncKVDeleteRangeHandler){
   		AsyncKVDeleteRangeHandler = handler;
	}
	if (!AsyncKVTxnHandler){
   		AsyncKVTxnHandler = handler;
	}
	if (!AsyncKVCompactHandler){
   		AsyncKVCompactHandler = handler;
	}
	if (!AsyncWatchWatchHandler){
   		AsyncWatchWatchHandler = handler;
	}
	if (!AsyncLeaseLeaseGrantHandler){
   		AsyncLeaseLeaseGrantHandler = handler;
	}
	if (!AsyncLeaseLeaseRevokeHandler){
   		AsyncLeaseLeaseRevokeHandler = handler;
	}
	if (!AsyncLeaseLeaseKeepAliveHandler){
   		AsyncLeaseLeaseKeepAliveHandler = handler;
	}
	if (!AsyncLeaseLeaseTimeToLiveHandler){
   		AsyncLeaseLeaseTimeToLiveHandler = handler;
	}
	if (!AsyncLeaseLeaseLeasesHandler){
   		AsyncLeaseLeaseLeasesHandler = handler;
	}
}


void InitEtcdStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity){
	registry.emplace<KVStubPtr>(nodeEntity, KV::NewStub(channel));
	registry.emplace<WatchStubPtr>(nodeEntity, Watch::NewStub(channel));
	registry.emplace<LeaseStubPtr>(nodeEntity, Lease::NewStub(channel));
}

}// namespace etcdserverpb
