#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/db_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace  {

using AccountDBServiceStubPtr = std::unique_ptr<AccountDBService::Stub>;



class AsyncAccountDBServiceLoad2RedisGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::LoadAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::LoadAccountResponse>> response_reader;
};

using AsyncAccountDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall>&)>;
extern AsyncAccountDBServiceLoad2RedisHandlerFunctionType AsyncAccountDBServiceLoad2RedisHandler;



class ::LoadAccountRequest;

void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request);
void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleAccountDBServiceCompletedQueueMessage(entt::registry& registry);
void InitAccountDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncAccountDBServiceSave2RedisGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::SaveAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::SaveAccountResponse>> response_reader;
};

using AsyncAccountDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall>&)>;
extern AsyncAccountDBServiceSave2RedisHandlerFunctionType AsyncAccountDBServiceSave2RedisHandler;



class ::SaveAccountRequest;

void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request);
void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleAccountDBServiceCompletedQueueMessage(entt::registry& registry);
void InitAccountDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

using PlayerDBServiceStubPtr = std::unique_ptr<PlayerDBService::Stub>;



class AsyncPlayerDBServiceLoad2RedisGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::LoadPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::LoadPlayerResponse>> response_reader;
};

using AsyncPlayerDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncPlayerDBServiceLoad2RedisGrpcClientCall>&)>;
extern AsyncPlayerDBServiceLoad2RedisHandlerFunctionType AsyncPlayerDBServiceLoad2RedisHandler;



class ::LoadPlayerRequest;

void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request);
void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandlePlayerDBServiceCompletedQueueMessage(entt::registry& registry);
void InitPlayerDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncPlayerDBServiceSave2RedisGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::SavePlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::SavePlayerResponse>> response_reader;
};

using AsyncPlayerDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncPlayerDBServiceSave2RedisGrpcClientCall>&)>;
extern AsyncPlayerDBServiceSave2RedisHandlerFunctionType AsyncPlayerDBServiceSave2RedisHandler;



class ::SavePlayerRequest;

void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request);
void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandlePlayerDBServiceCompletedQueueMessage(entt::registry& registry);
void InitPlayerDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

}// namespace 
