#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/db_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcAccountDBServiceStubPtr = std::unique_ptr<AccountDBService::Stub>;
class AsyncAccountDBServiceLoad2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    LoadAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< LoadAccountResponse>> response_reader;
};

class LoadAccountRequest;
void AccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const LoadAccountRequest& request);
class AsyncAccountDBServiceSave2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    SaveAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< SaveAccountResponse>> response_reader;
};

class SaveAccountRequest;
void AccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const SaveAccountRequest& request);

void HandleAccountDBServiceCompletedQueueMessage(entt::registry& registry	); 

void InitAccountDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
