#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/etcd/etcd.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using GrpcetcdserverpbKVStubPtr = std::unique_ptr<etcdserverpb::KV::Stub>;

class AsyncetcdserverpbKVRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::RangeResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::RangeResponse>> response_reader;
};

using AsyncetcdserverpbKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVRangeHandlerFunctionType  AsyncetcdserverpbKVRangeHandler;


class ::etcdserverpb::RangeRequest;
void SendetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::RangeRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbKVPutGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::PutResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::PutResponse>> response_reader;
};

using AsyncetcdserverpbKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>&)>;

extern AsyncetcdserverpbKVPutHandlerFunctionType  AsyncetcdserverpbKVPutHandler;


class ::etcdserverpb::PutRequest;
void SendetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::PutRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbKVDeleteRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::DeleteRangeResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::DeleteRangeResponse>> response_reader;
};

using AsyncetcdserverpbKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>&)>;

extern AsyncetcdserverpbKVDeleteRangeHandlerFunctionType  AsyncetcdserverpbKVDeleteRangeHandler;


class ::etcdserverpb::DeleteRangeRequest;
void SendetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::DeleteRangeRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbKVTxnGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::TxnResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::TxnResponse>> response_reader;
};

using AsyncetcdserverpbKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>&)>;

extern AsyncetcdserverpbKVTxnHandlerFunctionType  AsyncetcdserverpbKVTxnHandler;


class ::etcdserverpb::TxnRequest;
void SendetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::TxnRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbKVCompactGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::CompactionResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::CompactionResponse>> response_reader;
};

using AsyncetcdserverpbKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVCompactGrpcClientCall>&)>;

extern AsyncetcdserverpbKVCompactHandlerFunctionType  AsyncetcdserverpbKVCompactHandler;


class ::etcdserverpb::CompactionRequest;
void SendetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::CompactionRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
using GrpcetcdserverpbWatchStubPtr = std::unique_ptr<etcdserverpb::Watch::Stub>;

class AsyncetcdserverpbWatchWatchGrpcClient
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::WatchResponse reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<::etcdserverpb::WatchRequest,  ::etcdserverpb::WatchResponse>> stream;
	entt::entity nodeEntity{entt::null};
};


using AsyncetcdserverpbWatchWatchHandlerFunctionType = std::function<void(const ::etcdserverpb::WatchResponse&)>;

extern AsyncetcdserverpbWatchWatchHandlerFunctionType  AsyncetcdserverpbWatchWatchHandler;




class ::etcdserverpb::WatchRequest;
void SendetcdserverpbWatchWatch(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::WatchRequest& request);

void HandleetcdserverpbWatchCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbWatchCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
using GrpcetcdserverpbLeaseStubPtr = std::unique_ptr<etcdserverpb::Lease::Stub>;

class AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseGrantResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseGrantResponse>> response_reader;
};

using AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType  AsyncetcdserverpbLeaseLeaseGrantHandler;


class ::etcdserverpb::LeaseGrantRequest;
void SendetcdserverpbLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseGrantRequest& request);

void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseRevokeResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseRevokeResponse>> response_reader;
};

using AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType  AsyncetcdserverpbLeaseLeaseRevokeHandler;


class ::etcdserverpb::LeaseRevokeRequest;
void SendetcdserverpbLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseRevokeRequest& request);

void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClient
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseKeepAliveResponse reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<::etcdserverpb::LeaseKeepAliveRequest,  ::etcdserverpb::LeaseKeepAliveResponse>> stream;
	entt::entity nodeEntity{entt::null};
};


using AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const ::etcdserverpb::LeaseKeepAliveResponse&)>;

extern AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseKeepAliveHandler;




class ::etcdserverpb::LeaseKeepAliveRequest;
void SendetcdserverpbLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseKeepAliveRequest& request);

void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseTimeToLiveResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseTimeToLiveResponse>> response_reader;
};

using AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseTimeToLiveHandler;


class ::etcdserverpb::LeaseTimeToLiveRequest;
void SendetcdserverpbLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseTimeToLiveRequest& request);

void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::etcdserverpb::LeaseLeasesResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::etcdserverpb::LeaseLeasesResponse>> response_reader;
};

using AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall>&)>;

extern AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType  AsyncetcdserverpbLeaseLeaseLeasesHandler;


class ::etcdserverpb::LeaseLeasesRequest;
void SendetcdserverpbLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseLeasesRequest& request);

void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry	); 
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
