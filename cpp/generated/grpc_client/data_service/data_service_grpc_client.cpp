#include "muduo/base/Logging.h"

#include "data_service_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace data_service {
struct DataServiceCompleteQueue {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> tagPool;
#pragma region DataServiceLoadPlayerData
boost::object_pool<AsyncDataServiceLoadPlayerDataGrpcClient> DataServiceLoadPlayerDataPool;
using AsyncDataServiceLoadPlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::LoadPlayerDataResponse&)>;
AsyncDataServiceLoadPlayerDataHandlerFunctionType AsyncDataServiceLoadPlayerDataHandler;

void AsyncCompleteGrpcDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceLoadPlayerDataGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceLoadPlayerDataHandler) {
            AsyncDataServiceLoadPlayerDataHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceLoadPlayerDataPool.destroy(call);
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceLoadPlayerDataPool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoadPlayerData(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceLoadPlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceLoadPlayerDataPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoadPlayerData(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceLoadPlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::LoadPlayerDataRequest& derived = static_cast<const ::data_service::LoadPlayerDataRequest&>(message);
    SendDataServiceLoadPlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceSavePlayerData
boost::object_pool<AsyncDataServiceSavePlayerDataGrpcClient> DataServiceSavePlayerDataPool;
using AsyncDataServiceSavePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SavePlayerDataResponse&)>;
AsyncDataServiceSavePlayerDataHandlerFunctionType AsyncDataServiceSavePlayerDataHandler;

void AsyncCompleteGrpcDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceSavePlayerDataGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceSavePlayerDataHandler) {
            AsyncDataServiceSavePlayerDataHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceSavePlayerDataPool.destroy(call);
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceSavePlayerDataPool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSavePlayerData(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceSavePlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceSavePlayerDataPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSavePlayerData(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceSavePlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::SavePlayerDataRequest& derived = static_cast<const ::data_service::SavePlayerDataRequest&>(message);
    SendDataServiceSavePlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerField
boost::object_pool<AsyncDataServiceGetPlayerFieldGrpcClient> DataServiceGetPlayerFieldPool;
using AsyncDataServiceGetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerFieldResponse&)>;
AsyncDataServiceGetPlayerFieldHandlerFunctionType AsyncDataServiceGetPlayerFieldHandler;

void AsyncCompleteGrpcDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceGetPlayerFieldGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceGetPlayerFieldHandler) {
            AsyncDataServiceGetPlayerFieldHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceGetPlayerFieldPool.destroy(call);
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceGetPlayerFieldPool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncGetPlayerField(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerFieldMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceGetPlayerFieldPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncGetPlayerField(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerFieldMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::GetPlayerFieldRequest& derived = static_cast<const ::data_service::GetPlayerFieldRequest&>(message);
    SendDataServiceGetPlayerField(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceSetPlayerField
boost::object_pool<AsyncDataServiceSetPlayerFieldGrpcClient> DataServiceSetPlayerFieldPool;
using AsyncDataServiceSetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SetPlayerFieldResponse&)>;
AsyncDataServiceSetPlayerFieldHandlerFunctionType AsyncDataServiceSetPlayerFieldHandler;

void AsyncCompleteGrpcDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceSetPlayerFieldGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceSetPlayerFieldHandler) {
            AsyncDataServiceSetPlayerFieldHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceSetPlayerFieldPool.destroy(call);
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceSetPlayerFieldPool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSetPlayerField(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceSetPlayerFieldMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceSetPlayerFieldPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSetPlayerField(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceSetPlayerFieldMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::SetPlayerFieldRequest& derived = static_cast<const ::data_service::SetPlayerFieldRequest&>(message);
    SendDataServiceSetPlayerField(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRegisterPlayerZone
boost::object_pool<AsyncDataServiceRegisterPlayerZoneGrpcClient> DataServiceRegisterPlayerZonePool;
using AsyncDataServiceRegisterPlayerZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::google::protobuf::Empty&)>;
AsyncDataServiceRegisterPlayerZoneHandlerFunctionType AsyncDataServiceRegisterPlayerZoneHandler;

void AsyncCompleteGrpcDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceRegisterPlayerZoneGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceRegisterPlayerZoneHandler) {
            AsyncDataServiceRegisterPlayerZoneHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceRegisterPlayerZonePool.destroy(call);
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceRegisterPlayerZonePool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterPlayerZone(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRegisterPlayerZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceRegisterPlayerZonePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterPlayerZone(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRegisterPlayerZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::RegisterPlayerZoneRequest& derived = static_cast<const ::data_service::RegisterPlayerZoneRequest&>(message);
    SendDataServiceRegisterPlayerZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone
boost::object_pool<AsyncDataServiceGetPlayerHomeZoneGrpcClient> DataServiceGetPlayerHomeZonePool;
using AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerHomeZoneResponse&)>;
AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneHandler;

void AsyncCompleteGrpcDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceGetPlayerHomeZoneGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceGetPlayerHomeZoneHandler) {
            AsyncDataServiceGetPlayerHomeZoneHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceGetPlayerHomeZonePool.destroy(call);
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceGetPlayerHomeZonePool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncGetPlayerHomeZone(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerHomeZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceGetPlayerHomeZonePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncGetPlayerHomeZone(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerHomeZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::GetPlayerHomeZoneRequest& derived = static_cast<const ::data_service::GetPlayerHomeZoneRequest&>(message);
    SendDataServiceGetPlayerHomeZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone
boost::object_pool<AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient> DataServiceBatchGetPlayerHomeZonePool;
using AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchGetPlayerHomeZoneResponse&)>;
AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneHandler;

void AsyncCompleteGrpcDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceBatchGetPlayerHomeZoneHandler) {
            AsyncDataServiceBatchGetPlayerHomeZoneHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceBatchGetPlayerHomeZonePool.destroy(call);
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceBatchGetPlayerHomeZonePool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncBatchGetPlayerHomeZone(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceBatchGetPlayerHomeZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceBatchGetPlayerHomeZonePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncBatchGetPlayerHomeZone(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceBatchGetPlayerHomeZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::BatchGetPlayerHomeZoneRequest& derived = static_cast<const ::data_service::BatchGetPlayerHomeZoneRequest&>(message);
    SendDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceDeletePlayerData
boost::object_pool<AsyncDataServiceDeletePlayerDataGrpcClient> DataServiceDeletePlayerDataPool;
using AsyncDataServiceDeletePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::DeletePlayerDataResponse&)>;
AsyncDataServiceDeletePlayerDataHandlerFunctionType AsyncDataServiceDeletePlayerDataHandler;

void AsyncCompleteGrpcDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncDataServiceDeletePlayerDataGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncDataServiceDeletePlayerDataHandler) {
            AsyncDataServiceDeletePlayerDataHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	DataServiceDeletePlayerDataPool.destroy(call);
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(DataServiceDeletePlayerDataPool.construct());
    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncDeletePlayerData(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceDeletePlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(DataServiceDeletePlayerDataPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<DataServiceStubPtr>(nodeEntity)
        ->PrepareAsyncDeletePlayerData(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceDeletePlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::DeletePlayerDataRequest& derived = static_cast<const ::data_service::DeletePlayerDataRequest&>(message);
    SendDataServiceDeletePlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleDataServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
        switch (grpcTag->messageId) {
        case DataServiceLoadPlayerDataMessageId:
            AsyncCompleteGrpcDataServiceLoadPlayerData(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceSavePlayerDataMessageId:
            AsyncCompleteGrpcDataServiceSavePlayerData(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceGetPlayerFieldMessageId:
            AsyncCompleteGrpcDataServiceGetPlayerField(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceSetPlayerFieldMessageId:
            AsyncCompleteGrpcDataServiceSetPlayerField(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceRegisterPlayerZoneMessageId:
            AsyncCompleteGrpcDataServiceRegisterPlayerZone(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceGetPlayerHomeZoneMessageId:
            AsyncCompleteGrpcDataServiceGetPlayerHomeZone(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceBatchGetPlayerHomeZoneMessageId:
            AsyncCompleteGrpcDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case DataServiceDeletePlayerDataMessageId:
            AsyncCompleteGrpcDataServiceDeletePlayerData(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        default:
            break;
        }
}

void SetDataServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncDataServiceLoadPlayerDataHandler = handler;
    AsyncDataServiceSavePlayerDataHandler = handler;
    AsyncDataServiceGetPlayerFieldHandler = handler;
    AsyncDataServiceSetPlayerFieldHandler = handler;
    AsyncDataServiceRegisterPlayerZoneHandler = handler;
    AsyncDataServiceGetPlayerHomeZoneHandler = handler;
    AsyncDataServiceBatchGetPlayerHomeZoneHandler = handler;
    AsyncDataServiceDeletePlayerDataHandler = handler;
}

void SetDataServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncDataServiceLoadPlayerDataHandler) {
        AsyncDataServiceLoadPlayerDataHandler = handler;
    }
    if (!AsyncDataServiceSavePlayerDataHandler) {
        AsyncDataServiceSavePlayerDataHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerFieldHandler) {
        AsyncDataServiceGetPlayerFieldHandler = handler;
    }
    if (!AsyncDataServiceSetPlayerFieldHandler) {
        AsyncDataServiceSetPlayerFieldHandler = handler;
    }
    if (!AsyncDataServiceRegisterPlayerZoneHandler) {
        AsyncDataServiceRegisterPlayerZoneHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerHomeZoneHandler) {
        AsyncDataServiceGetPlayerHomeZoneHandler = handler;
    }
    if (!AsyncDataServiceBatchGetPlayerHomeZoneHandler) {
        AsyncDataServiceBatchGetPlayerHomeZoneHandler = handler;
    }
    if (!AsyncDataServiceDeletePlayerDataHandler) {
        AsyncDataServiceDeletePlayerDataHandler = handler;
    }
}

void InitDataServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<DataServiceStubPtr>(nodeEntity, DataService::NewStub(channel));

}

}// namespace data_service
