#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/etcd/etcd.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace etcdserverpb {

using KVStubPtr = std::unique_ptr<KV::Stub>;



class AsyncKVRangeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::RangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::RangeResponse>> response_reader;
};

using AsyncKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVRangeGrpcClientCall>&)>;
extern AsyncKVRangeHandlerFunctionType AsyncKVRangeHandler;



class ::etcdserverpb::RangeRequest;

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request);
void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleKVCompletedQueueMessage(entt::registry& registry);
void InitKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncKVPutGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::PutResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::PutResponse>> response_reader;
};

using AsyncKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVPutGrpcClientCall>&)>;
extern AsyncKVPutHandlerFunctionType AsyncKVPutHandler;



class ::etcdserverpb::PutRequest;

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request);
void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleKVCompletedQueueMessage(entt::registry& registry);
void InitKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncKVDeleteRangeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::DeleteRangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::DeleteRangeResponse>> response_reader;
};

using AsyncKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVDeleteRangeGrpcClientCall>&)>;
extern AsyncKVDeleteRangeHandlerFunctionType AsyncKVDeleteRangeHandler;



class ::etcdserverpb::DeleteRangeRequest;

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request);
void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleKVCompletedQueueMessage(entt::registry& registry);
void InitKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncKVTxnGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::TxnResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::TxnResponse>> response_reader;
};

using AsyncKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVTxnGrpcClientCall>&)>;
extern AsyncKVTxnHandlerFunctionType AsyncKVTxnHandler;



class ::etcdserverpb::TxnRequest;

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request);
void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleKVCompletedQueueMessage(entt::registry& registry);
void InitKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncKVCompactGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::CompactionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::CompactionResponse>> response_reader;
};

using AsyncKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncKVCompactGrpcClientCall>&)>;
extern AsyncKVCompactHandlerFunctionType AsyncKVCompactHandler;



class ::etcdserverpb::CompactionRequest;

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request);
void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleKVCompletedQueueMessage(entt::registry& registry);
void InitKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

using WatchStubPtr = std::unique_ptr<Watch::Stub>;



class AsyncWatchWatchGrpcClient {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::WatchResponse reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<::etcdserverpb::WatchRequest, ::etcdserverpb::WatchResponse>> stream;
};

struct WatchRequestBuffer {
    boost::circular_buffer<::etcdserverpb::WatchRequest> pendingWritesBuffer{200};
};

struct WatchRequestWriteInProgress {
    bool isInProgress{false};
};

using AsyncWatchWatchHandlerFunctionType = std::function<void(const ::etcdserverpb::WatchResponse&)>;
extern AsyncWatchWatchHandlerFunctionType AsyncWatchWatchHandler;



class ::etcdserverpb::WatchRequest;

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request);
void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleWatchCompletedQueueMessage(entt::registry& registry);
void InitWatchCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

using LeaseStubPtr = std::unique_ptr<Lease::Stub>;



class AsyncLeaseLeaseGrantGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseGrantResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseGrantResponse>> response_reader;
};

using AsyncLeaseLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseGrantGrpcClientCall>&)>;
extern AsyncLeaseLeaseGrantHandlerFunctionType AsyncLeaseLeaseGrantHandler;



class ::etcdserverpb::LeaseGrantRequest;

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request);
void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLeaseCompletedQueueMessage(entt::registry& registry);
void InitLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncLeaseLeaseRevokeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseRevokeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseRevokeResponse>> response_reader;
};

using AsyncLeaseLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseRevokeGrpcClientCall>&)>;
extern AsyncLeaseLeaseRevokeHandlerFunctionType AsyncLeaseLeaseRevokeHandler;



class ::etcdserverpb::LeaseRevokeRequest;

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request);
void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLeaseCompletedQueueMessage(entt::registry& registry);
void InitLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncLeaseLeaseKeepAliveGrpcClient {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseKeepAliveResponse reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<::etcdserverpb::LeaseKeepAliveRequest, ::etcdserverpb::LeaseKeepAliveResponse>> stream;
};

struct LeaseKeepAliveRequestBuffer {
    boost::circular_buffer<::etcdserverpb::LeaseKeepAliveRequest> pendingWritesBuffer{200};
};

struct LeaseKeepAliveRequestWriteInProgress {
    bool isInProgress{false};
};

using AsyncLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const ::etcdserverpb::LeaseKeepAliveResponse&)>;
extern AsyncLeaseLeaseKeepAliveHandlerFunctionType AsyncLeaseLeaseKeepAliveHandler;



class ::etcdserverpb::LeaseKeepAliveRequest;

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request);
void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLeaseCompletedQueueMessage(entt::registry& registry);
void InitLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncLeaseLeaseTimeToLiveGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseTimeToLiveResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseTimeToLiveResponse>> response_reader;
};

using AsyncLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseTimeToLiveGrpcClientCall>&)>;
extern AsyncLeaseLeaseTimeToLiveHandlerFunctionType AsyncLeaseLeaseTimeToLiveHandler;



class ::etcdserverpb::LeaseTimeToLiveRequest;

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request);
void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLeaseCompletedQueueMessage(entt::registry& registry);
void InitLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncLeaseLeaseLeasesGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseLeasesResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseLeasesResponse>> response_reader;
};

using AsyncLeaseLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLeaseLeaseLeasesGrpcClientCall>&)>;
extern AsyncLeaseLeaseLeasesHandlerFunctionType AsyncLeaseLeaseLeasesHandler;



class ::etcdserverpb::LeaseLeasesRequest;

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request);
void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLeaseCompletedQueueMessage(entt::registry& registry);
void InitLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

}// namespace etcdserverpb
