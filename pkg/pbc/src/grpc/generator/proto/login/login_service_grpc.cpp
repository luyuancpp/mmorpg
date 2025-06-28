#include "muduo/base/Logging.h"
#include "login_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"
#include <boost/pool/object_pool.hpp>



namespace loginpb {
struct LoginServiceCompleteQueue {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> pool;
#pragma region ClientPlayerLoginLogin
boost::object_pool<AsyncClientPlayerLoginLoginGrpcClient> ClientPlayerLoginLoginPool;
using AsyncClientPlayerLoginLoginHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::LoginResponse&)>;
AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;

void AsyncCompleteGrpcClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncClientPlayerLoginLoginGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginLoginHandler) {
            AsyncClientPlayerLoginLoginHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	ClientPlayerLoginLoginPool.destroy(call);
}



void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request) {

    auto call(ClientPlayerLoginLoginPool.construct());
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(pool.construct(GrpcMethod::ClientPlayerLogin_Login, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(ClientPlayerLoginLoginPool.construct());

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::loginpb::LoginRequest& derived = static_cast<const ::loginpb::LoginRequest&>(message);
    SendClientPlayerLoginLogin(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginCreatePlayer
boost::object_pool<AsyncClientPlayerLoginCreatePlayerGrpcClient> ClientPlayerLoginCreatePlayerPool;
using AsyncClientPlayerLoginCreatePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::CreatePlayerResponse&)>;
AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;

void AsyncCompleteGrpcClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncClientPlayerLoginCreatePlayerGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginCreatePlayerHandler) {
            AsyncClientPlayerLoginCreatePlayerHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	ClientPlayerLoginCreatePlayerPool.destroy(call);
}



void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request) {

    auto call(ClientPlayerLoginCreatePlayerPool.construct());
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(pool.construct(GrpcMethod::ClientPlayerLogin_CreatePlayer, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(ClientPlayerLoginCreatePlayerPool.construct());

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::loginpb::CreatePlayerRequest& derived = static_cast<const ::loginpb::CreatePlayerRequest&>(message);
    SendClientPlayerLoginCreatePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginEnterGame
boost::object_pool<AsyncClientPlayerLoginEnterGameGrpcClient> ClientPlayerLoginEnterGamePool;
using AsyncClientPlayerLoginEnterGameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::EnterGameResponse&)>;
AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;

void AsyncCompleteGrpcClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncClientPlayerLoginEnterGameGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginEnterGameHandler) {
            AsyncClientPlayerLoginEnterGameHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	ClientPlayerLoginEnterGamePool.destroy(call);
}



void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request) {

    auto call(ClientPlayerLoginEnterGamePool.construct());
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(pool.construct(GrpcMethod::ClientPlayerLogin_EnterGame, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(ClientPlayerLoginEnterGamePool.construct());

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::loginpb::EnterGameRequest& derived = static_cast<const ::loginpb::EnterGameRequest&>(message);
    SendClientPlayerLoginEnterGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginLeaveGame
boost::object_pool<AsyncClientPlayerLoginLeaveGameGrpcClient> ClientPlayerLoginLeaveGamePool;
using AsyncClientPlayerLoginLeaveGameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;

void AsyncCompleteGrpcClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncClientPlayerLoginLeaveGameGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginLeaveGameHandler) {
            AsyncClientPlayerLoginLeaveGameHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	ClientPlayerLoginLeaveGamePool.destroy(call);
}



void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request) {

    auto call(ClientPlayerLoginLeaveGamePool.construct());
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(pool.construct(GrpcMethod::ClientPlayerLogin_LeaveGame, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(ClientPlayerLoginLeaveGamePool.construct());

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::loginpb::LeaveGameRequest& derived = static_cast<const ::loginpb::LeaveGameRequest&>(message);
    SendClientPlayerLoginLeaveGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginDisconnect
boost::object_pool<AsyncClientPlayerLoginDisconnectGrpcClient> ClientPlayerLoginDisconnectPool;
using AsyncClientPlayerLoginDisconnectHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;

void AsyncCompleteGrpcClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncClientPlayerLoginDisconnectGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginDisconnectHandler) {
            AsyncClientPlayerLoginDisconnectHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	ClientPlayerLoginDisconnectPool.destroy(call);
}



void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request) {

    auto call(ClientPlayerLoginDisconnectPool.construct());
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(pool.construct(GrpcMethod::ClientPlayerLogin_Disconnect, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(ClientPlayerLoginDisconnectPool.construct());

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                           &registry.get<LoginServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::loginpb::LoginNodeDisconnectRequest& derived = static_cast<const ::loginpb::LoginNodeDisconnectRequest&>(message);
    SendClientPlayerLoginDisconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion


void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<LoginServiceCompleteQueue>(nodeEntity);











}


void HandleLoginServiceCompletedQueueMessage(entt::registry& registry) {


    auto&& view = registry.view<LoginServiceCompleteQueue>();
    for (auto&& [e, completeQueueComp] : view.each()) {
        void* got_tag = nullptr;
        bool ok = false;
        gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
        if (grpc::CompletionQueue::GOT_EVENT != completeQueueComp.cq.AsyncNext(&got_tag, &ok, tm)) {
            return;
        }
        if (!ok) {
            LOG_ERROR << "RPC failed";
            return;
        }
        GrpcTag* grpcTag(reinterpret_cast<GrpcTag*>(got_tag));

        switch (grpcTag->type) {
        case GrpcMethod::ClientPlayerLogin_Login:
            AsyncCompleteGrpcClientPlayerLoginLogin(registry, e, completeQueueComp.cq, grpcTag->valuePtr);
            break;
        case GrpcMethod::ClientPlayerLogin_CreatePlayer:
            AsyncCompleteGrpcClientPlayerLoginCreatePlayer(registry, e, completeQueueComp.cq, grpcTag->valuePtr);
            break;
        case GrpcMethod::ClientPlayerLogin_EnterGame:
            AsyncCompleteGrpcClientPlayerLoginEnterGame(registry, e, completeQueueComp.cq, grpcTag->valuePtr);
            break;
        case GrpcMethod::ClientPlayerLogin_LeaveGame:
            AsyncCompleteGrpcClientPlayerLoginLeaveGame(registry, e, completeQueueComp.cq, grpcTag->valuePtr);
            break;
        case GrpcMethod::ClientPlayerLogin_Disconnect:
            AsyncCompleteGrpcClientPlayerLoginDisconnect(registry, e, completeQueueComp.cq, grpcTag->valuePtr);
            break;
        default:
            break;
        }

		pool.destroy(grpcTag);
    }
}



void SetLoginServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncClientPlayerLoginLoginHandler = handler;
    AsyncClientPlayerLoginCreatePlayerHandler = handler;
    AsyncClientPlayerLoginEnterGameHandler = handler;
    AsyncClientPlayerLoginLeaveGameHandler = handler;
    AsyncClientPlayerLoginDisconnectHandler = handler;
}


void SetLoginServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncClientPlayerLoginLoginHandler) {
        AsyncClientPlayerLoginLoginHandler = handler;
    }
    if (!AsyncClientPlayerLoginCreatePlayerHandler) {
        AsyncClientPlayerLoginCreatePlayerHandler = handler;
    }
    if (!AsyncClientPlayerLoginEnterGameHandler) {
        AsyncClientPlayerLoginEnterGameHandler = handler;
    }
    if (!AsyncClientPlayerLoginLeaveGameHandler) {
        AsyncClientPlayerLoginLeaveGameHandler = handler;
    }
    if (!AsyncClientPlayerLoginDisconnectHandler) {
        AsyncClientPlayerLoginDisconnectHandler = handler;
    }
}


void InitLoginServiceStub(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<ClientPlayerLoginStubPtr>(nodeEntity, ClientPlayerLogin::NewStub(channel));
}


}// namespace loginpb
