#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "proto/etcd/etcd.grpc.pb.h"


using namespace etcdserverpb;

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcetcdserverpbKVStubPtr = std::unique_ptr<KV::Stub>;
class AsyncKVRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RangeResponse>> response_reader;
};

class RangeRequest;
void KVRange(entt::registry& registry, entt::entity nodeEntity, const etcdserverpb::RangeRequest& request);
class AsyncKVPutGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    PutResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< PutResponse>> response_reader;
};

class PutRequest;
void KVPut(entt::registry& registry, entt::entity nodeEntity, const PutRequest& request);
class AsyncKVDeleteRangeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    DeleteRangeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< DeleteRangeResponse>> response_reader;
};

class DeleteRangeRequest;
void KVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const DeleteRangeRequest& request);
class AsyncKVTxnGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    TxnResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< TxnResponse>> response_reader;
};

class TxnRequest;
void KVTxn(entt::registry& registry, entt::entity nodeEntity, const TxnRequest& request);
class AsyncKVCompactGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    CompactionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< CompactionResponse>> response_reader;
};

class CompactionRequest;
void KVCompact(entt::registry& registry, entt::entity nodeEntity, const CompactionRequest& request);

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry	); 

void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
