#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/db_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcAccountDBServiceStubPtr = std::unique_ptr<::AccountDBService::Stub>;
class AsyncAccountDBServiceLoad2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::LoadPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::LoadPlayerResponse>> response_reader;
};

class ::LoadPlayerRequest;
void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const  ::LoadPlayerRequest& request);

using AsyncAccountDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall>&)>;

extern AsyncAccountDBServiceLoad2RedisHandlerFunctionType  AsyncAccountDBServiceLoad2RedisHandler;;
class AsyncAccountDBServiceSave2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::SavePlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::SavePlayerResponse>> response_reader;
};

class ::SavePlayerRequest;
void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const  ::SavePlayerRequest& request);

using AsyncAccountDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall>&)>;

extern AsyncAccountDBServiceSave2RedisHandlerFunctionType  AsyncAccountDBServiceSave2RedisHandler;;
class AsyncAccountDBServiceLoad2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::LoadAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::LoadAccountResponse>> response_reader;
};

class ::LoadAccountRequest;
void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const  ::LoadAccountRequest& request);

using AsyncAccountDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall>&)>;

extern AsyncAccountDBServiceLoad2RedisHandlerFunctionType  AsyncAccountDBServiceLoad2RedisHandler;;
class AsyncAccountDBServiceSave2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::SaveAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::SaveAccountResponse>> response_reader;
};

class ::SaveAccountRequest;
void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const  ::SaveAccountRequest& request);

using AsyncAccountDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall>&)>;

extern AsyncAccountDBServiceSave2RedisHandlerFunctionType  AsyncAccountDBServiceSave2RedisHandler;;

void HandleAccountDBServiceCompletedQueueMessage(entt::registry& registry	); 

void InitAccountDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);


