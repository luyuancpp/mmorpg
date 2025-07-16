#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc/grpc_tag.h"

#include "proto/db/db_service.grpc.pb.h"


#include "service_info/db_service_service_info.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace  {
using AccountDBServiceStubPtr = std::unique_ptr<AccountDBService::Stub>;
#pragma region AccountDBServiceLoad2Redis


struct AsyncAccountDBServiceLoad2RedisGrpcClient {
    uint32_t messageId{ AccountDBServiceLoad2RedisMessageId };
    ClientContext context;
    Status status;
    ::LoadAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::LoadAccountResponse>> response_reader;
};



class ::LoadAccountRequest;
using AsyncAccountDBServiceLoad2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::LoadAccountResponse&)>;
extern AsyncAccountDBServiceLoad2RedisHandlerFunctionType AsyncAccountDBServiceLoad2RedisHandler;

void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request);
void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region AccountDBServiceSave2Redis


struct AsyncAccountDBServiceSave2RedisGrpcClient {
    uint32_t messageId{ AccountDBServiceSave2RedisMessageId };
    ClientContext context;
    Status status;
    ::SaveAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::SaveAccountResponse>> response_reader;
};



class ::SaveAccountRequest;
using AsyncAccountDBServiceSave2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::SaveAccountResponse&)>;
extern AsyncAccountDBServiceSave2RedisHandlerFunctionType AsyncAccountDBServiceSave2RedisHandler;

void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request);
void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
using PlayerDBServiceStubPtr = std::unique_ptr<PlayerDBService::Stub>;
#pragma region PlayerDBServiceLoad2Redis


struct AsyncPlayerDBServiceLoad2RedisGrpcClient {
    uint32_t messageId{ PlayerDBServiceLoad2RedisMessageId };
    ClientContext context;
    Status status;
    ::LoadPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::LoadPlayerResponse>> response_reader;
};



class ::LoadPlayerRequest;
using AsyncPlayerDBServiceLoad2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::LoadPlayerResponse&)>;
extern AsyncPlayerDBServiceLoad2RedisHandlerFunctionType AsyncPlayerDBServiceLoad2RedisHandler;

void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request);
void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region PlayerDBServiceSave2Redis


struct AsyncPlayerDBServiceSave2RedisGrpcClient {
    uint32_t messageId{ PlayerDBServiceSave2RedisMessageId };
    ClientContext context;
    Status status;
    ::SavePlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::SavePlayerResponse>> response_reader;
};



class ::SavePlayerRequest;
using AsyncPlayerDBServiceSave2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::SavePlayerResponse&)>;
extern AsyncPlayerDBServiceSave2RedisHandlerFunctionType AsyncPlayerDBServiceSave2RedisHandler;

void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request);
void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetDbServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDbServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleDbServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitDbServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace 

