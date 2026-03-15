#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
#include "proto/data_service/data_service.grpc.pb.h"

#include "rpc/service_metadata/data_service_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace data_service {
using DataServiceStubPtr = std::unique_ptr<DataService::Stub>;
#pragma region DataServiceLoadPlayerData

struct AsyncDataServiceLoadPlayerDataGrpcClient {
    uint32_t messageId{ DataServiceLoadPlayerDataMessageId };
    ClientContext context;
    Status status;
    ::data_service::LoadPlayerDataResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::LoadPlayerDataResponse>> response_reader;
};

class ::data_service::LoadPlayerDataRequest;
using AsyncDataServiceLoadPlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::LoadPlayerDataResponse&)>;
extern AsyncDataServiceLoadPlayerDataHandlerFunctionType AsyncDataServiceLoadPlayerDataHandler;

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request);
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceSavePlayerData

struct AsyncDataServiceSavePlayerDataGrpcClient {
    uint32_t messageId{ DataServiceSavePlayerDataMessageId };
    ClientContext context;
    Status status;
    ::data_service::SavePlayerDataResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::SavePlayerDataResponse>> response_reader;
};

class ::data_service::SavePlayerDataRequest;
using AsyncDataServiceSavePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SavePlayerDataResponse&)>;
extern AsyncDataServiceSavePlayerDataHandlerFunctionType AsyncDataServiceSavePlayerDataHandler;

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request);
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceGetPlayerField

struct AsyncDataServiceGetPlayerFieldGrpcClient {
    uint32_t messageId{ DataServiceGetPlayerFieldMessageId };
    ClientContext context;
    Status status;
    ::data_service::GetPlayerFieldResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::GetPlayerFieldResponse>> response_reader;
};

class ::data_service::GetPlayerFieldRequest;
using AsyncDataServiceGetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerFieldResponse&)>;
extern AsyncDataServiceGetPlayerFieldHandlerFunctionType AsyncDataServiceGetPlayerFieldHandler;

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request);
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceSetPlayerField

struct AsyncDataServiceSetPlayerFieldGrpcClient {
    uint32_t messageId{ DataServiceSetPlayerFieldMessageId };
    ClientContext context;
    Status status;
    ::data_service::SetPlayerFieldResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::SetPlayerFieldResponse>> response_reader;
};

class ::data_service::SetPlayerFieldRequest;
using AsyncDataServiceSetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SetPlayerFieldResponse&)>;
extern AsyncDataServiceSetPlayerFieldHandlerFunctionType AsyncDataServiceSetPlayerFieldHandler;

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request);
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceRegisterPlayerZone

struct AsyncDataServiceRegisterPlayerZoneGrpcClient {
    uint32_t messageId{ DataServiceRegisterPlayerZoneMessageId };
    ClientContext context;
    Status status;
    ::google::protobuf::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::google::protobuf::Empty>> response_reader;
};

class ::data_service::RegisterPlayerZoneRequest;
using AsyncDataServiceRegisterPlayerZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::google::protobuf::Empty&)>;
extern AsyncDataServiceRegisterPlayerZoneHandlerFunctionType AsyncDataServiceRegisterPlayerZoneHandler;

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request);
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone

struct AsyncDataServiceGetPlayerHomeZoneGrpcClient {
    uint32_t messageId{ DataServiceGetPlayerHomeZoneMessageId };
    ClientContext context;
    Status status;
    ::data_service::GetPlayerHomeZoneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::GetPlayerHomeZoneResponse>> response_reader;
};

class ::data_service::GetPlayerHomeZoneRequest;
using AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerHomeZoneResponse&)>;
extern AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneHandler;

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request);
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone

struct AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient {
    uint32_t messageId{ DataServiceBatchGetPlayerHomeZoneMessageId };
    ClientContext context;
    Status status;
    ::data_service::BatchGetPlayerHomeZoneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::BatchGetPlayerHomeZoneResponse>> response_reader;
};

class ::data_service::BatchGetPlayerHomeZoneRequest;
using AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchGetPlayerHomeZoneResponse&)>;
extern AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneHandler;

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request);
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region DataServiceDeletePlayerData

struct AsyncDataServiceDeletePlayerDataGrpcClient {
    uint32_t messageId{ DataServiceDeletePlayerDataMessageId };
    ClientContext context;
    Status status;
    ::data_service::DeletePlayerDataResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::DeletePlayerDataResponse>> response_reader;
};

class ::data_service::DeletePlayerDataRequest;
using AsyncDataServiceDeletePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::DeletePlayerDataResponse&)>;
extern AsyncDataServiceDeletePlayerDataHandlerFunctionType AsyncDataServiceDeletePlayerDataHandler;

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request);
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetDataServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDataServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleDataServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitDataServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace data_service
