#pragma once
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

     etcdserverpb::RangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::RangeResponse>> response_reader;
};

class RangeRequest;
void etcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::RangeRequest& request);
class AsyncetcdserverpbKVPutGrpcClientCall
{
public:
    ClientContext context;
    Status status;

     etcdserverpb::PutResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::PutResponse>> response_reader;
};

class PutRequest;
void etcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::PutRequest& request);
class AsyncetcdserverpbKVDeleteRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

     etcdserverpb::DeleteRangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::DeleteRangeResponse>> response_reader;
};

class DeleteRangeRequest;
void etcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::DeleteRangeRequest& request);
class AsyncetcdserverpbKVTxnGrpcClientCall
{
public:
    ClientContext context;
    Status status;

     etcdserverpb::TxnResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::TxnResponse>> response_reader;
};

class TxnRequest;
void etcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::TxnRequest& request);
class AsyncetcdserverpbKVCompactGrpcClientCall
{
public:
    ClientContext context;
    Status status;

     etcdserverpb::CompactionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  etcdserverpb::CompactionResponse>> response_reader;
};

class CompactionRequest;
void etcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::CompactionRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 

void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
