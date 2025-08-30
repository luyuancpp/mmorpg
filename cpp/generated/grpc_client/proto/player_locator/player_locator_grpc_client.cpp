#include "muduo/base/Logging.h"


#include "player_locator_grpc_client.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "common/util/encode/base64.h"
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace playerlocator {
struct PlayerLocatorCompleteQueue {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> tagPool;
#pragma region PlayerLocatorSetLocation
boost::object_pool<AsyncPlayerLocatorSetLocationGrpcClient> PlayerLocatorSetLocationPool;
using AsyncPlayerLocatorSetLocationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncPlayerLocatorSetLocationHandlerFunctionType AsyncPlayerLocatorSetLocationHandler;

void AsyncCompleteGrpcPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncPlayerLocatorSetLocationGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerLocatorSetLocationHandler) {
            AsyncPlayerLocatorSetLocationHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	PlayerLocatorSetLocationPool.destroy(call);
}



void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(PlayerLocatorSetLocationPool.construct());
    call->response_reader = registry
        .get<PlayerLocatorStubPtr>(nodeEntity)
        ->PrepareAsyncSetLocation(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerLocatorSetLocationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(PlayerLocatorSetLocationPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<PlayerLocatorStubPtr>(nodeEntity)
        ->PrepareAsyncSetLocation(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerLocatorSetLocationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::playerlocator::PlayerLocation& derived = static_cast<const ::playerlocator::PlayerLocation&>(message);
    SendPlayerLocatorSetLocation(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorGetLocation
boost::object_pool<AsyncPlayerLocatorGetLocationGrpcClient> PlayerLocatorGetLocationPool;
using AsyncPlayerLocatorGetLocationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::playerlocator::PlayerLocation&)>;
AsyncPlayerLocatorGetLocationHandlerFunctionType AsyncPlayerLocatorGetLocationHandler;

void AsyncCompleteGrpcPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncPlayerLocatorGetLocationGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerLocatorGetLocationHandler) {
            AsyncPlayerLocatorGetLocationHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	PlayerLocatorGetLocationPool.destroy(call);
}



void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(PlayerLocatorGetLocationPool.construct());
    call->response_reader = registry
        .get<PlayerLocatorStubPtr>(nodeEntity)
        ->PrepareAsyncGetLocation(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerLocatorGetLocationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(PlayerLocatorGetLocationPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<PlayerLocatorStubPtr>(nodeEntity)
        ->PrepareAsyncGetLocation(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerLocatorGetLocationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::playerlocator::PlayerId& derived = static_cast<const ::playerlocator::PlayerId&>(message);
    SendPlayerLocatorGetLocation(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorMarkOffline
boost::object_pool<AsyncPlayerLocatorMarkOfflineGrpcClient> PlayerLocatorMarkOfflinePool;
using AsyncPlayerLocatorMarkOfflineHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncPlayerLocatorMarkOfflineHandlerFunctionType AsyncPlayerLocatorMarkOfflineHandler;

void AsyncCompleteGrpcPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncPlayerLocatorMarkOfflineGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerLocatorMarkOfflineHandler) {
            AsyncPlayerLocatorMarkOfflineHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	PlayerLocatorMarkOfflinePool.destroy(call);
}



void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(PlayerLocatorMarkOfflinePool.construct());
    call->response_reader = registry
        .get<PlayerLocatorStubPtr>(nodeEntity)
        ->PrepareAsyncMarkOffline(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerLocatorMarkOfflineMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(PlayerLocatorMarkOfflinePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<PlayerLocatorStubPtr>(nodeEntity)
        ->PrepareAsyncMarkOffline(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerLocatorMarkOfflineMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::playerlocator::PlayerId& derived = static_cast<const ::playerlocator::PlayerId&>(message);
    SendPlayerLocatorMarkOffline(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion


void HandlePlayerLocatorCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
        switch (grpcTag->messageId) {
        case PlayerLocatorSetLocationMessageId:
            AsyncCompleteGrpcPlayerLocatorSetLocation(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case PlayerLocatorGetLocationMessageId:
            AsyncCompleteGrpcPlayerLocatorGetLocation(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case PlayerLocatorMarkOfflineMessageId:
            AsyncCompleteGrpcPlayerLocatorMarkOffline(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        default:
            break;
        }
}



void SetPlayerLocatorHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncPlayerLocatorSetLocationHandler = handler;
    AsyncPlayerLocatorGetLocationHandler = handler;
    AsyncPlayerLocatorMarkOfflineHandler = handler;
}


void SetPlayerLocatorIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncPlayerLocatorSetLocationHandler) {
        AsyncPlayerLocatorSetLocationHandler = handler;
    }
    if (!AsyncPlayerLocatorGetLocationHandler) {
        AsyncPlayerLocatorGetLocationHandler = handler;
    }
    if (!AsyncPlayerLocatorMarkOfflineHandler) {
        AsyncPlayerLocatorMarkOfflineHandler = handler;
    }
}


void InitPlayerLocatorGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<PlayerLocatorStubPtr>(nodeEntity, PlayerLocator::NewStub(channel));

}


}// namespace playerlocator
