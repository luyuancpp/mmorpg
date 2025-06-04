#include "muduo/base/Logging.h"
#include "login_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"

namespace loginpb{
#pragma region ClientPlayerLoginLogin
struct ClientPlayerLoginLoginCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncClientPlayerLoginLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginLoginGrpcClientCall>&)>;
AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;

void AsyncCompleteGrpcClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncClientPlayerLoginLoginGrpcClientCall> call(
        static_cast<AsyncClientPlayerLoginLoginGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginLoginHandler) {
            AsyncClientPlayerLoginLoginHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request) {

    AsyncClientPlayerLoginLoginGrpcClientCall* call = new AsyncClientPlayerLoginLoginGrpcClientCall;
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                  &registry.get<ClientPlayerLoginLoginCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncClientPlayerLoginLoginGrpcClientCall* call = new AsyncClientPlayerLoginLoginGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                  &registry.get<ClientPlayerLoginLoginCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::LoginC2LRequest& derived = static_cast<const ::loginpb::LoginC2LRequest&>(message);
	SendClientPlayerLoginLogin(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region ClientPlayerLoginCreatePlayer
struct ClientPlayerLoginCreatePlayerCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncClientPlayerLoginCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginCreatePlayerGrpcClientCall>&)>;
AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;

void AsyncCompleteGrpcClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncClientPlayerLoginCreatePlayerGrpcClientCall> call(
        static_cast<AsyncClientPlayerLoginCreatePlayerGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginCreatePlayerHandler) {
            AsyncClientPlayerLoginCreatePlayerHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request) {

    AsyncClientPlayerLoginCreatePlayerGrpcClientCall* call = new AsyncClientPlayerLoginCreatePlayerGrpcClientCall;
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                  &registry.get<ClientPlayerLoginCreatePlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncClientPlayerLoginCreatePlayerGrpcClientCall* call = new AsyncClientPlayerLoginCreatePlayerGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                  &registry.get<ClientPlayerLoginCreatePlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::CreatePlayerC2LRequest& derived = static_cast<const ::loginpb::CreatePlayerC2LRequest&>(message);
	SendClientPlayerLoginCreatePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region ClientPlayerLoginEnterGame
struct ClientPlayerLoginEnterGameCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncClientPlayerLoginEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginEnterGameGrpcClientCall>&)>;
AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;

void AsyncCompleteGrpcClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncClientPlayerLoginEnterGameGrpcClientCall> call(
        static_cast<AsyncClientPlayerLoginEnterGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginEnterGameHandler) {
            AsyncClientPlayerLoginEnterGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request) {

    AsyncClientPlayerLoginEnterGameGrpcClientCall* call = new AsyncClientPlayerLoginEnterGameGrpcClientCall;
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                  &registry.get<ClientPlayerLoginEnterGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncClientPlayerLoginEnterGameGrpcClientCall* call = new AsyncClientPlayerLoginEnterGameGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                  &registry.get<ClientPlayerLoginEnterGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::EnterGameC2LRequest& derived = static_cast<const ::loginpb::EnterGameC2LRequest&>(message);
	SendClientPlayerLoginEnterGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region ClientPlayerLoginLeaveGame
struct ClientPlayerLoginLeaveGameCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncClientPlayerLoginLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginLeaveGameGrpcClientCall>&)>;
AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;

void AsyncCompleteGrpcClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncClientPlayerLoginLeaveGameGrpcClientCall> call(
        static_cast<AsyncClientPlayerLoginLeaveGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginLeaveGameHandler) {
            AsyncClientPlayerLoginLeaveGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request) {

    AsyncClientPlayerLoginLeaveGameGrpcClientCall* call = new AsyncClientPlayerLoginLeaveGameGrpcClientCall;
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                  &registry.get<ClientPlayerLoginLeaveGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncClientPlayerLoginLeaveGameGrpcClientCall* call = new AsyncClientPlayerLoginLeaveGameGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                  &registry.get<ClientPlayerLoginLeaveGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::LeaveGameC2LRequest& derived = static_cast<const ::loginpb::LeaveGameC2LRequest&>(message);
	SendClientPlayerLoginLeaveGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region ClientPlayerLoginDisconnect
struct ClientPlayerLoginDisconnectCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncClientPlayerLoginDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginDisconnectGrpcClientCall>&)>;
AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;

void AsyncCompleteGrpcClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncClientPlayerLoginDisconnectGrpcClientCall> call(
        static_cast<AsyncClientPlayerLoginDisconnectGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncClientPlayerLoginDisconnectHandler) {
            AsyncClientPlayerLoginDisconnectHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request) {

    AsyncClientPlayerLoginDisconnectGrpcClientCall* call = new AsyncClientPlayerLoginDisconnectGrpcClientCall;
    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                  &registry.get<ClientPlayerLoginDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncClientPlayerLoginDisconnectGrpcClientCall* call = new AsyncClientPlayerLoginDisconnectGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<ClientPlayerLoginStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                  &registry.get<ClientPlayerLoginDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::LoginNodeDisconnectRequest& derived = static_cast<const ::loginpb::LoginNodeDisconnectRequest&>(message);
	SendClientPlayerLoginDisconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



void InitClientPlayerLoginCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<ClientPlayerLoginLoginCompleteQueue>(nodeEntity);

    registry.emplace<ClientPlayerLoginCreatePlayerCompleteQueue>(nodeEntity);

    registry.emplace<ClientPlayerLoginEnterGameCompleteQueue>(nodeEntity);

    registry.emplace<ClientPlayerLoginLeaveGameCompleteQueue>(nodeEntity);

    registry.emplace<ClientPlayerLoginDisconnectCompleteQueue>(nodeEntity);
}
void HandleClientPlayerLoginCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<ClientPlayerLoginLoginCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcClientPlayerLoginLogin(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<ClientPlayerLoginCreatePlayerCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcClientPlayerLoginCreatePlayer(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<ClientPlayerLoginEnterGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcClientPlayerLoginEnterGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<ClientPlayerLoginLeaveGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcClientPlayerLoginLeaveGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<ClientPlayerLoginDisconnectCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcClientPlayerLoginDisconnect(registry, e, completeQueueComp.cq);
        }
    }}}// namespace loginpb
