#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/etcd/etcd.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcetcdserverpbLeaseStubPtr = std::unique_ptr<etcdserverpb::Lease::Stub>;
class AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::LeaseGrantResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::LeaseGrantResponse>> response_reader;
};

class LeaseGrantRequest;
void SendetcdserverpbLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseGrantRequest& request);

using AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType  AsyncetcdserverpbLeaseLeaseGrantHandler;;
class AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::LeaseRevokeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::LeaseRevokeResponse>> response_reader;
};

class LeaseRevokeRequest;
void SendetcdserverpbLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseRevokeRequest& request);

using AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType  AsyncetcdserverpbLeaseLeaseRevokeHandler;;
class AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::LeaseKeepAliveResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::LeaseKeepAliveResponse>> response_reader;
};

class LeaseKeepAliveRequest;
void SendetcdserverpbLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseKeepAliveRequest& request);

using AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseKeepAliveHandler;;
class AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::LeaseTimeToLiveResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::LeaseTimeToLiveResponse>> response_reader;
};

class LeaseTimeToLiveRequest;
void SendetcdserverpbLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseTimeToLiveRequest& request);

using AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseTimeToLiveHandler;;
class AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::LeaseLeasesResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::LeaseLeasesResponse>> response_reader;
};

class LeaseLeasesRequest;
void SendetcdserverpbLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseLeasesRequest& request);

using AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType  AsyncetcdserverpbLeaseLeaseLeasesHandler;;
class AsyncetcdserverpbWatchWatchGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::WatchResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::WatchResponse>> response_reader;
};

class WatchRequest;
void SendetcdserverpbWatchWatch(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::WatchRequest& request);

using AsyncetcdserverpbWatchWatchHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbWatchWatchGrpcClientCall>&)>;

extern AsyncetcdserverpbWatchWatchHandlerFunctionType  AsyncetcdserverpbWatchWatchHandler;;
class AsyncetcdserverpbKVRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::RangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::RangeResponse>> response_reader;
};

class RangeRequest;
void SendetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::RangeRequest& request);

using AsyncetcdserverpbKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVRangeHandlerFunctionType  AsyncetcdserverpbKVRangeHandler;;
class AsyncetcdserverpbKVPutGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::PutResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::PutResponse>> response_reader;
};

class PutRequest;
void SendetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::PutRequest& request);

using AsyncetcdserverpbKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>&)>;

extern AsyncetcdserverpbKVPutHandlerFunctionType  AsyncetcdserverpbKVPutHandler;;
class AsyncetcdserverpbKVDeleteRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::DeleteRangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::DeleteRangeResponse>> response_reader;
};

class DeleteRangeRequest;
void SendetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::DeleteRangeRequest& request);

using AsyncetcdserverpbKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVDeleteRangeHandlerFunctionType  AsyncetcdserverpbKVDeleteRangeHandler;;
class AsyncetcdserverpbKVTxnGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::TxnResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::TxnResponse>> response_reader;
};

class TxnRequest;
void SendetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::TxnRequest& request);

using AsyncetcdserverpbKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>&)>;

extern AsyncetcdserverpbKVTxnHandlerFunctionType  AsyncetcdserverpbKVTxnHandler;;
class AsyncetcdserverpbKVCompactGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    etcdserverpb::CompactionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::CompactionResponse>> response_reader;
};

class CompactionRequest;
void SendetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::CompactionRequest& request);

using AsyncetcdserverpbKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVCompactGrpcClientCall>&)>;

extern AsyncetcdserverpbKVCompactHandlerFunctionType  AsyncetcdserverpbKVCompactHandler;;

void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry	); 

void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);


