#include "muduo/base/Logging.h"


#include "db_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"
#include <boost/pool/object_pool.hpp>


namespace  {
struct DbServiceCompleteQueue {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> tagPool;
#pragma region AccountDBServiceLoad2Redis
boost::object_pool<AsyncAccountDBServiceLoad2RedisGrpcClient> AccountDBServiceLoad2RedisPool;
using AsyncAccountDBServiceLoad2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::LoadAccountResponse&)>;
AsyncAccountDBServiceLoad2RedisHandlerFunctionType AsyncAccountDBServiceLoad2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncAccountDBServiceLoad2RedisGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncAccountDBServiceLoad2RedisHandler) {
            AsyncAccountDBServiceLoad2RedisHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	AccountDBServiceLoad2RedisPool.destroy(call);
}



void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(AccountDBServiceLoad2RedisPool.construct());
    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(AccountDBServiceLoad2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(AccountDBServiceLoad2RedisPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(AccountDBServiceLoad2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::LoadAccountRequest& derived = static_cast<const ::LoadAccountRequest&>(message);
    SendAccountDBServiceLoad2Redis(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region AccountDBServiceSave2Redis
boost::object_pool<AsyncAccountDBServiceSave2RedisGrpcClient> AccountDBServiceSave2RedisPool;
using AsyncAccountDBServiceSave2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::SaveAccountResponse&)>;
AsyncAccountDBServiceSave2RedisHandlerFunctionType AsyncAccountDBServiceSave2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncAccountDBServiceSave2RedisGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncAccountDBServiceSave2RedisHandler) {
            AsyncAccountDBServiceSave2RedisHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	AccountDBServiceSave2RedisPool.destroy(call);
}



void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(AccountDBServiceSave2RedisPool.construct());
    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(AccountDBServiceSave2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(AccountDBServiceSave2RedisPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(AccountDBServiceSave2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::SaveAccountRequest& derived = static_cast<const ::SaveAccountRequest&>(message);
    SendAccountDBServiceSave2Redis(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerDBServiceLoad2Redis
boost::object_pool<AsyncPlayerDBServiceLoad2RedisGrpcClient> PlayerDBServiceLoad2RedisPool;
using AsyncPlayerDBServiceLoad2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::LoadPlayerResponse&)>;
AsyncPlayerDBServiceLoad2RedisHandlerFunctionType AsyncPlayerDBServiceLoad2RedisHandler;

void AsyncCompleteGrpcPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncPlayerDBServiceLoad2RedisGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerDBServiceLoad2RedisHandler) {
            AsyncPlayerDBServiceLoad2RedisHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	PlayerDBServiceLoad2RedisPool.destroy(call);
}



void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(PlayerDBServiceLoad2RedisPool.construct());
    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerDBServiceLoad2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(PlayerDBServiceLoad2RedisPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerDBServiceLoad2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::LoadPlayerRequest& derived = static_cast<const ::LoadPlayerRequest&>(message);
    SendPlayerDBServiceLoad2Redis(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerDBServiceSave2Redis
boost::object_pool<AsyncPlayerDBServiceSave2RedisGrpcClient> PlayerDBServiceSave2RedisPool;
using AsyncPlayerDBServiceSave2RedisHandlerFunctionType =
    std::function<void(const ClientContext&, const ::SavePlayerResponse&)>;
AsyncPlayerDBServiceSave2RedisHandlerFunctionType AsyncPlayerDBServiceSave2RedisHandler;

void AsyncCompleteGrpcPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncPlayerDBServiceSave2RedisGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerDBServiceSave2RedisHandler) {
            AsyncPlayerDBServiceSave2RedisHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	PlayerDBServiceSave2RedisPool.destroy(call);
}



void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(PlayerDBServiceSave2RedisPool.construct());
    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerDBServiceSave2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(PlayerDBServiceSave2RedisPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerDBServiceSave2RedisMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::SavePlayerRequest& derived = static_cast<const ::SavePlayerRequest&>(message);
    SendPlayerDBServiceSave2Redis(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion


void HandleDbServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
        switch (grpcTag->messageId) {
        case AccountDBServiceLoad2RedisMessageId:
            AsyncCompleteGrpcAccountDBServiceLoad2Redis(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case AccountDBServiceSave2RedisMessageId:
            AsyncCompleteGrpcAccountDBServiceSave2Redis(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case PlayerDBServiceLoad2RedisMessageId:
            AsyncCompleteGrpcPlayerDBServiceLoad2Redis(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case PlayerDBServiceSave2RedisMessageId:
            AsyncCompleteGrpcPlayerDBServiceSave2Redis(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        default:
            break;
        }
}



void SetDbServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {


    AsyncAccountDBServiceLoad2RedisHandler = handler;
    AsyncAccountDBServiceSave2RedisHandler = handler;
    AsyncPlayerDBServiceLoad2RedisHandler = handler;
    AsyncPlayerDBServiceSave2RedisHandler = handler;
}


void SetDbServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {


    if (!AsyncAccountDBServiceLoad2RedisHandler) {
        AsyncAccountDBServiceLoad2RedisHandler = handler;
    }
    if (!AsyncAccountDBServiceSave2RedisHandler) {
        AsyncAccountDBServiceSave2RedisHandler = handler;
    }
    if (!AsyncPlayerDBServiceLoad2RedisHandler) {
        AsyncPlayerDBServiceLoad2RedisHandler = handler;
    }
    if (!AsyncPlayerDBServiceSave2RedisHandler) {
        AsyncPlayerDBServiceSave2RedisHandler = handler;
    }
}


void InitDbServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {


    registry.emplace<AccountDBServiceStubPtr>(nodeEntity, AccountDBService::NewStub(channel));
    registry.emplace<PlayerDBServiceStubPtr>(nodeEntity, PlayerDBService::NewStub(channel));

}


}// namespace 
