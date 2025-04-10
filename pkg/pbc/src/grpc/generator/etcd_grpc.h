#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/etcd/etcd.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcetcdserverpbKVStubPtr = std::unique_ptr<etcdserverpb::KV::Stub>;
class AsyncetcdserverpbKVLeaseGrantGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseGrantResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::LeaseGrantResponse>> response_reader;
};

class ::etcdserverpb::LeaseGrantRequest;
void SendetcdserverpbKVLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseGrantRequest& request);

using AsyncetcdserverpbKVLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseGrantGrpcClientCall>&)>;

extern AsyncetcdserverpbKVLeaseGrantHandlerFunctionType  AsyncetcdserverpbKVLeaseGrantHandler;;
class AsyncetcdserverpbKVLeaseRevokeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseRevokeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::LeaseRevokeResponse>> response_reader;
};

class ::etcdserverpb::LeaseRevokeRequest;
void SendetcdserverpbKVLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseRevokeRequest& request);

using AsyncetcdserverpbKVLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseRevokeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVLeaseRevokeHandlerFunctionType  AsyncetcdserverpbKVLeaseRevokeHandler;;
class AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseKeepAliveResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::LeaseKeepAliveResponse>> response_reader;
};

class ::etcdserverpb::LeaseKeepAliveRequest;
void SendetcdserverpbKVLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseKeepAliveRequest& request);

using AsyncetcdserverpbKVLeaseKeepAliveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall>&)>;

extern AsyncetcdserverpbKVLeaseKeepAliveHandlerFunctionType  AsyncetcdserverpbKVLeaseKeepAliveHandler;;
class AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseTimeToLiveResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::LeaseTimeToLiveResponse>> response_reader;
};

class ::etcdserverpb::LeaseTimeToLiveRequest;
void SendetcdserverpbKVLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseTimeToLiveRequest& request);

using AsyncetcdserverpbKVLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall>&)>;

extern AsyncetcdserverpbKVLeaseTimeToLiveHandlerFunctionType  AsyncetcdserverpbKVLeaseTimeToLiveHandler;;
class AsyncetcdserverpbKVLeaseLeasesGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseLeasesResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::LeaseLeasesResponse>> response_reader;
};

class ::etcdserverpb::LeaseLeasesRequest;
void SendetcdserverpbKVLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseLeasesRequest& request);

using AsyncetcdserverpbKVLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseLeasesGrpcClientCall>&)>;

extern AsyncetcdserverpbKVLeaseLeasesHandlerFunctionType  AsyncetcdserverpbKVLeaseLeasesHandler;;
class AsyncetcdserverpbKVWatchGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::WatchResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::WatchResponse>> response_reader;
};

class ::etcdserverpb::WatchRequest;
void SendetcdserverpbKVWatch(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::WatchRequest& request);

using AsyncetcdserverpbKVWatchHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVWatchGrpcClientCall>&)>;

extern AsyncetcdserverpbKVWatchHandlerFunctionType  AsyncetcdserverpbKVWatchHandler;;
class AsyncetcdserverpbKVRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::RangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::RangeResponse>> response_reader;
};

class ::etcdserverpb::RangeRequest;
void SendetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::RangeRequest& request);

using AsyncetcdserverpbKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVRangeHandlerFunctionType  AsyncetcdserverpbKVRangeHandler;;
class AsyncetcdserverpbKVPutGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::PutResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::PutResponse>> response_reader;
};

class ::etcdserverpb::PutRequest;
void SendetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::PutRequest& request);

using AsyncetcdserverpbKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>&)>;

extern AsyncetcdserverpbKVPutHandlerFunctionType  AsyncetcdserverpbKVPutHandler;;
class AsyncetcdserverpbKVDeleteRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::DeleteRangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::DeleteRangeResponse>> response_reader;
};

class ::etcdserverpb::DeleteRangeRequest;
void SendetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::DeleteRangeRequest& request);

using AsyncetcdserverpbKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVDeleteRangeHandlerFunctionType  AsyncetcdserverpbKVDeleteRangeHandler;;
class AsyncetcdserverpbKVTxnGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::TxnResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::TxnResponse>> response_reader;
};

class ::etcdserverpb::TxnRequest;
void SendetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::TxnRequest& request);

using AsyncetcdserverpbKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>&)>;

extern AsyncetcdserverpbKVTxnHandlerFunctionType  AsyncetcdserverpbKVTxnHandler;;
class AsyncetcdserverpbKVCompactGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::CompactionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::etcdserverpb::CompactionResponse>> response_reader;
};

class ::etcdserverpb::CompactionRequest;
void SendetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::CompactionRequest& request);

using AsyncetcdserverpbKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVCompactGrpcClientCall>&)>;

extern AsyncetcdserverpbKVCompactHandlerFunctionType  AsyncetcdserverpbKVCompactHandler;;

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 

void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);


