#include "muduo/base/Logging.h"
#include "login_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

namespace loginpb{

struct loginpbLoginServiceLoginCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncloginpbLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceLoginGrpcClientCall>&)>;
AsyncloginpbLoginServiceLoginHandlerFunctionType AsyncloginpbLoginServiceLoginHandler;

void AsyncCompleteGrpcloginpbLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncloginpbLoginServiceLoginGrpcClientCall> call(
        static_cast<AsyncloginpbLoginServiceLoginGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncloginpbLoginServiceLoginHandler) {
            AsyncloginpbLoginServiceLoginHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendloginpbLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request) {

    AsyncloginpbLoginServiceLoginGrpcClientCall* call = new AsyncloginpbLoginServiceLoginGrpcClientCall;
    call->response_reader = registry
        .get<GrpcloginpbLoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLogin(&call->context, request,
                                  &registry.get<loginpbLoginServiceLoginCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct loginpbLoginServiceCreatePlayerCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncloginpbLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceCreatePlayerGrpcClientCall>&)>;
AsyncloginpbLoginServiceCreatePlayerHandlerFunctionType AsyncloginpbLoginServiceCreatePlayerHandler;

void AsyncCompleteGrpcloginpbLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncloginpbLoginServiceCreatePlayerGrpcClientCall> call(
        static_cast<AsyncloginpbLoginServiceCreatePlayerGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncloginpbLoginServiceCreatePlayerHandler) {
            AsyncloginpbLoginServiceCreatePlayerHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendloginpbLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request) {

    AsyncloginpbLoginServiceCreatePlayerGrpcClientCall* call = new AsyncloginpbLoginServiceCreatePlayerGrpcClientCall;
    call->response_reader = registry
        .get<GrpcloginpbLoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCreatePlayer(&call->context, request,
                                  &registry.get<loginpbLoginServiceCreatePlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct loginpbLoginServiceEnterGameCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncloginpbLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceEnterGameGrpcClientCall>&)>;
AsyncloginpbLoginServiceEnterGameHandlerFunctionType AsyncloginpbLoginServiceEnterGameHandler;

void AsyncCompleteGrpcloginpbLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncloginpbLoginServiceEnterGameGrpcClientCall> call(
        static_cast<AsyncloginpbLoginServiceEnterGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncloginpbLoginServiceEnterGameHandler) {
            AsyncloginpbLoginServiceEnterGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendloginpbLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request) {

    AsyncloginpbLoginServiceEnterGameGrpcClientCall* call = new AsyncloginpbLoginServiceEnterGameGrpcClientCall;
    call->response_reader = registry
        .get<GrpcloginpbLoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGame(&call->context, request,
                                  &registry.get<loginpbLoginServiceEnterGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct loginpbLoginServiceLeaveGameCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncloginpbLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceLeaveGameGrpcClientCall>&)>;
AsyncloginpbLoginServiceLeaveGameHandlerFunctionType AsyncloginpbLoginServiceLeaveGameHandler;

void AsyncCompleteGrpcloginpbLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncloginpbLoginServiceLeaveGameGrpcClientCall> call(
        static_cast<AsyncloginpbLoginServiceLeaveGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncloginpbLoginServiceLeaveGameHandler) {
            AsyncloginpbLoginServiceLeaveGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendloginpbLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request) {

    AsyncloginpbLoginServiceLeaveGameGrpcClientCall* call = new AsyncloginpbLoginServiceLeaveGameGrpcClientCall;
    call->response_reader = registry
        .get<GrpcloginpbLoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveGame(&call->context, request,
                                  &registry.get<loginpbLoginServiceLeaveGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct loginpbLoginServiceDisconnectCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncloginpbLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceDisconnectGrpcClientCall>&)>;
AsyncloginpbLoginServiceDisconnectHandlerFunctionType AsyncloginpbLoginServiceDisconnectHandler;

void AsyncCompleteGrpcloginpbLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncloginpbLoginServiceDisconnectGrpcClientCall> call(
        static_cast<AsyncloginpbLoginServiceDisconnectGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncloginpbLoginServiceDisconnectHandler) {
            AsyncloginpbLoginServiceDisconnectHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendloginpbLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request) {

    AsyncloginpbLoginServiceDisconnectGrpcClientCall* call = new AsyncloginpbLoginServiceDisconnectGrpcClientCall;
    call->response_reader = registry
        .get<GrpcloginpbLoginServiceStubPtr>(nodeEntity)
        ->PrepareAsyncDisconnect(&call->context, request,
                                  &registry.get<loginpbLoginServiceDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
void InitloginpbLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<loginpbLoginServiceLoginCompleteQueue>(nodeEntity);

    registry.emplace<loginpbLoginServiceCreatePlayerCompleteQueue>(nodeEntity);

    registry.emplace<loginpbLoginServiceEnterGameCompleteQueue>(nodeEntity);

    registry.emplace<loginpbLoginServiceLeaveGameCompleteQueue>(nodeEntity);

    registry.emplace<loginpbLoginServiceDisconnectCompleteQueue>(nodeEntity);

}
void HandleloginpbLoginServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<loginpbLoginServiceLoginCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcloginpbLoginServiceLogin(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<loginpbLoginServiceCreatePlayerCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcloginpbLoginServiceCreatePlayer(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<loginpbLoginServiceEnterGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcloginpbLoginServiceEnterGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<loginpbLoginServiceLeaveGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcloginpbLoginServiceLeaveGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<loginpbLoginServiceDisconnectCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcloginpbLoginServiceDisconnect(registry, e, completeQueueComp.cq);
        }
    }
}

}// namespace loginpb
