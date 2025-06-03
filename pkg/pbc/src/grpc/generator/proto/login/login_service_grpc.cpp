#include "muduo/base/Logging.h"
#include "login_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"

namespace loginpb{
#pragma region LoginServiceLogin
struct LoginServiceLoginCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)>;
AsyncLoginServiceLoginHandlerFunctionType AsyncLoginServiceLoginHandler;

void AsyncCompleteGrpcLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall> call(
        static_cast<AsyncLoginServiceLoginGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLoginServiceLoginHandler) {
            AsyncLoginServiceLoginHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request) {

    AsyncLoginServiceLoginGrpcClientCall* call = new AsyncLoginServiceLoginGrpcClientCall;
    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                  &registry.get<LoginServiceLoginCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLoginServiceLoginGrpcClientCall* call = new AsyncLoginServiceLoginGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                  &registry.get<LoginServiceLoginCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::LoginC2LRequest& derived = static_cast<const ::loginpb::LoginC2LRequest&>(message);
	SendLoginServiceLogin(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LoginServiceCreatePlayer
struct LoginServiceCreatePlayerCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)>;
AsyncLoginServiceCreatePlayerHandlerFunctionType AsyncLoginServiceCreatePlayerHandler;

void AsyncCompleteGrpcLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall> call(
        static_cast<AsyncLoginServiceCreatePlayerGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLoginServiceCreatePlayerHandler) {
            AsyncLoginServiceCreatePlayerHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request) {

    AsyncLoginServiceCreatePlayerGrpcClientCall* call = new AsyncLoginServiceCreatePlayerGrpcClientCall;
    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                  &registry.get<LoginServiceCreatePlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLoginServiceCreatePlayerGrpcClientCall* call = new AsyncLoginServiceCreatePlayerGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                  &registry.get<LoginServiceCreatePlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::CreatePlayerC2LRequest& derived = static_cast<const ::loginpb::CreatePlayerC2LRequest&>(message);
	SendLoginServiceCreatePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LoginServiceEnterGame
struct LoginServiceEnterGameCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)>;
AsyncLoginServiceEnterGameHandlerFunctionType AsyncLoginServiceEnterGameHandler;

void AsyncCompleteGrpcLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall> call(
        static_cast<AsyncLoginServiceEnterGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLoginServiceEnterGameHandler) {
            AsyncLoginServiceEnterGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request) {

    AsyncLoginServiceEnterGameGrpcClientCall* call = new AsyncLoginServiceEnterGameGrpcClientCall;
    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                  &registry.get<LoginServiceEnterGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLoginServiceEnterGameGrpcClientCall* call = new AsyncLoginServiceEnterGameGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                  &registry.get<LoginServiceEnterGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::EnterGameC2LRequest& derived = static_cast<const ::loginpb::EnterGameC2LRequest&>(message);
	SendLoginServiceEnterGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LoginServiceLeaveGame
struct LoginServiceLeaveGameCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)>;
AsyncLoginServiceLeaveGameHandlerFunctionType AsyncLoginServiceLeaveGameHandler;

void AsyncCompleteGrpcLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall> call(
        static_cast<AsyncLoginServiceLeaveGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLoginServiceLeaveGameHandler) {
            AsyncLoginServiceLeaveGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request) {

    AsyncLoginServiceLeaveGameGrpcClientCall* call = new AsyncLoginServiceLeaveGameGrpcClientCall;
    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                  &registry.get<LoginServiceLeaveGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLoginServiceLeaveGameGrpcClientCall* call = new AsyncLoginServiceLeaveGameGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                  &registry.get<LoginServiceLeaveGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::LeaveGameC2LRequest& derived = static_cast<const ::loginpb::LeaveGameC2LRequest&>(message);
	SendLoginServiceLeaveGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



#pragma region LoginServiceDisconnect
struct LoginServiceDisconnectCompleteQueue {
    grpc::CompletionQueue cq;
};

using AsyncLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall>&)>;
AsyncLoginServiceDisconnectHandlerFunctionType AsyncLoginServiceDisconnectHandler;

void AsyncCompleteGrpcLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall> call(
        static_cast<AsyncLoginServiceDisconnectGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncLoginServiceDisconnectHandler) {
            AsyncLoginServiceDisconnectHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}

void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request) {

    AsyncLoginServiceDisconnectGrpcClientCall* call = new AsyncLoginServiceDisconnectGrpcClientCall;
    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                  &registry.get<LoginServiceDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncLoginServiceDisconnectGrpcClientCall* call = new AsyncLoginServiceDisconnectGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<LoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                  &registry.get<LoginServiceDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const ::loginpb::LoginNodeDisconnectRequest& derived = static_cast<const ::loginpb::LoginNodeDisconnectRequest&>(message);
	SendLoginServiceDisconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion



void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<LoginServiceLoginCompleteQueue>(nodeEntity);

    registry.emplace<LoginServiceCreatePlayerCompleteQueue>(nodeEntity);

    registry.emplace<LoginServiceEnterGameCompleteQueue>(nodeEntity);

    registry.emplace<LoginServiceLeaveGameCompleteQueue>(nodeEntity);

    registry.emplace<LoginServiceDisconnectCompleteQueue>(nodeEntity);
}
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<LoginServiceLoginCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLoginServiceLogin(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LoginServiceCreatePlayerCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLoginServiceCreatePlayer(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LoginServiceEnterGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLoginServiceEnterGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LoginServiceLeaveGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLoginServiceLeaveGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<LoginServiceDisconnectCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcLoginServiceDisconnect(registry, e, completeQueueComp.cq);
        }
    }}}// namespace loginpb
