#include "muduo/base/Logging.h"
#include "centre_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

namespace {

struct CentreServiceGatePlayerServiceCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceGatePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall>&)>;
AsyncCentreServiceGatePlayerServiceHandlerFunctionType AsyncCentreServiceGatePlayerServiceHandler;

void AsyncCompleteGrpcCentreServiceGatePlayerService(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall> call(
        static_cast<AsyncCentreServiceGatePlayerServiceGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceGatePlayerServiceHandler) {
            AsyncCentreServiceGatePlayerServiceHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceGatePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::GateClientMessageRequest& request) {

    AsyncCentreServiceGatePlayerServiceGrpcClientCall* call = new AsyncCentreServiceGatePlayerServiceGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncGatePlayerService(&call->context, request,
                                  &registry.get<CentreServiceGatePlayerServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceGateSessionDisconnectCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceGateSessionDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall>&)>;
AsyncCentreServiceGateSessionDisconnectHandlerFunctionType AsyncCentreServiceGateSessionDisconnectHandler;

void AsyncCompleteGrpcCentreServiceGateSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall> call(
        static_cast<AsyncCentreServiceGateSessionDisconnectGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceGateSessionDisconnectHandler) {
            AsyncCentreServiceGateSessionDisconnectHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceGateSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::GateSessionDisconnectRequest& request) {

    AsyncCentreServiceGateSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceGateSessionDisconnectGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncGateSessionDisconnect(&call->context, request,
                                  &registry.get<CentreServiceGateSessionDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceLoginNodeAccountLoginCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceLoginNodeAccountLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall>&)>;
AsyncCentreServiceLoginNodeAccountLoginHandlerFunctionType AsyncCentreServiceLoginNodeAccountLoginHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall> call(
        static_cast<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceLoginNodeAccountLoginHandler) {
            AsyncCentreServiceLoginNodeAccountLoginHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceLoginNodeAccountLogin(entt::registry& registry, entt::entity nodeEntity, const ::LoginRequest& request) {

    AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall* call = new AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoginNodeAccountLogin(&call->context, request,
                                  &registry.get<CentreServiceLoginNodeAccountLoginCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceLoginNodeEnterGameCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceLoginNodeEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall>&)>;
AsyncCentreServiceLoginNodeEnterGameHandlerFunctionType AsyncCentreServiceLoginNodeEnterGameHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeEnterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall> call(
        static_cast<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceLoginNodeEnterGameHandler) {
            AsyncCentreServiceLoginNodeEnterGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceLoginNodeEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::CentrePlayerGameNodeEntryRequest& request) {

    AsyncCentreServiceLoginNodeEnterGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeEnterGameGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoginNodeEnterGame(&call->context, request,
                                  &registry.get<CentreServiceLoginNodeEnterGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceLoginNodeLeaveGameCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceLoginNodeLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall>&)>;
AsyncCentreServiceLoginNodeLeaveGameHandlerFunctionType AsyncCentreServiceLoginNodeLeaveGameHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall> call(
        static_cast<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceLoginNodeLeaveGameHandler) {
            AsyncCentreServiceLoginNodeLeaveGameHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceLoginNodeLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::LoginNodeLeaveGameRequest& request) {

    AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoginNodeLeaveGame(&call->context, request,
                                  &registry.get<CentreServiceLoginNodeLeaveGameCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceLoginNodeSessionDisconnectCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceLoginNodeSessionDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall>&)>;
AsyncCentreServiceLoginNodeSessionDisconnectHandlerFunctionType AsyncCentreServiceLoginNodeSessionDisconnectHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall> call(
        static_cast<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceLoginNodeSessionDisconnectHandler) {
            AsyncCentreServiceLoginNodeSessionDisconnectHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceLoginNodeSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::GateSessionDisconnectRequest& request) {

    AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoginNodeSessionDisconnect(&call->context, request,
                                  &registry.get<CentreServiceLoginNodeSessionDisconnectCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServicePlayerServiceCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServicePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall>&)>;
AsyncCentreServicePlayerServiceHandlerFunctionType AsyncCentreServicePlayerServiceHandler;

void AsyncCompleteGrpcCentreServicePlayerService(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall> call(
        static_cast<AsyncCentreServicePlayerServiceGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServicePlayerServiceHandler) {
            AsyncCentreServicePlayerServiceHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServicePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request) {

    AsyncCentreServicePlayerServiceGrpcClientCall* call = new AsyncCentreServicePlayerServiceGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncPlayerService(&call->context, request,
                                  &registry.get<CentreServicePlayerServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceEnterGsSucceedCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceEnterGsSucceedHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall>&)>;
AsyncCentreServiceEnterGsSucceedHandlerFunctionType AsyncCentreServiceEnterGsSucceedHandler;

void AsyncCompleteGrpcCentreServiceEnterGsSucceed(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall> call(
        static_cast<AsyncCentreServiceEnterGsSucceedGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceEnterGsSucceedHandler) {
            AsyncCentreServiceEnterGsSucceedHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceEnterGsSucceed(entt::registry& registry, entt::entity nodeEntity, const ::EnterGameNodeSuccessRequest& request) {

    AsyncCentreServiceEnterGsSucceedGrpcClientCall* call = new AsyncCentreServiceEnterGsSucceedGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterGsSucceed(&call->context, request,
                                  &registry.get<CentreServiceEnterGsSucceedCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceRouteNodeStringMsgCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceRouteNodeStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall>&)>;
AsyncCentreServiceRouteNodeStringMsgHandlerFunctionType AsyncCentreServiceRouteNodeStringMsgHandler;

void AsyncCompleteGrpcCentreServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall> call(
        static_cast<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceRouteNodeStringMsgHandler) {
            AsyncCentreServiceRouteNodeStringMsgHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request) {

    AsyncCentreServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncCentreServiceRouteNodeStringMsgGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRouteNodeStringMsg(&call->context, request,
                                  &registry.get<CentreServiceRouteNodeStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceRoutePlayerStringMsgCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceRoutePlayerStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall>&)>;
AsyncCentreServiceRoutePlayerStringMsgHandlerFunctionType AsyncCentreServiceRoutePlayerStringMsgHandler;

void AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall> call(
        static_cast<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceRoutePlayerStringMsgHandler) {
            AsyncCentreServiceRoutePlayerStringMsgHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request) {

    AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
                                  &registry.get<CentreServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceInitSceneNodeCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceInitSceneNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceInitSceneNodeGrpcClientCall>&)>;
AsyncCentreServiceInitSceneNodeHandlerFunctionType AsyncCentreServiceInitSceneNodeHandler;

void AsyncCompleteGrpcCentreServiceInitSceneNode(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceInitSceneNodeGrpcClientCall> call(
        static_cast<AsyncCentreServiceInitSceneNodeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceInitSceneNodeHandler) {
            AsyncCentreServiceInitSceneNodeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceInitSceneNode(entt::registry& registry, entt::entity nodeEntity, const ::InitSceneNodeRequest& request) {

    AsyncCentreServiceInitSceneNodeGrpcClientCall* call = new AsyncCentreServiceInitSceneNodeGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncInitSceneNode(&call->context, request,
                                  &registry.get<CentreServiceInitSceneNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct CentreServiceRegisterNodeSessionCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncCentreServiceRegisterNodeSessionHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterNodeSessionGrpcClientCall>&)>;
AsyncCentreServiceRegisterNodeSessionHandlerFunctionType AsyncCentreServiceRegisterNodeSessionHandler;

void AsyncCompleteGrpcCentreServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncCentreServiceRegisterNodeSessionGrpcClientCall> call(
        static_cast<AsyncCentreServiceRegisterNodeSessionGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncCentreServiceRegisterNodeSessionHandler) {
            AsyncCentreServiceRegisterNodeSessionHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendCentreServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request) {

    AsyncCentreServiceRegisterNodeSessionGrpcClientCall* call = new AsyncCentreServiceRegisterNodeSessionGrpcClientCall;
    call->response_reader = registry
        .get<CentreServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterNodeSession(&call->context, request,
                                  &registry.get<CentreServiceRegisterNodeSessionCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<CentreServiceGatePlayerServiceCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceGateSessionDisconnectCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceLoginNodeAccountLoginCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceLoginNodeEnterGameCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceLoginNodeLeaveGameCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceLoginNodeSessionDisconnectCompleteQueue>(nodeEntity);

    registry.emplace<CentreServicePlayerServiceCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceEnterGsSucceedCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceRouteNodeStringMsgCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceInitSceneNodeCompleteQueue>(nodeEntity);

    registry.emplace<CentreServiceRegisterNodeSessionCompleteQueue>(nodeEntity);

}
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<CentreServiceGatePlayerServiceCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceGatePlayerService(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceGateSessionDisconnectCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceGateSessionDisconnect(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceLoginNodeAccountLoginCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceLoginNodeEnterGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceLoginNodeEnterGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceLoginNodeLeaveGameCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceLoginNodeSessionDisconnectCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServicePlayerServiceCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServicePlayerService(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceEnterGsSucceedCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceEnterGsSucceed(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceRouteNodeStringMsgCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceRouteNodeStringMsg(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceRoutePlayerStringMsgCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceInitSceneNodeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceInitSceneNode(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<CentreServiceRegisterNodeSessionCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcCentreServiceRegisterNodeSession(registry, e, completeQueueComp.cq);
        }
    }
}

}// namespace 
