#include "muduo/base/Logging.h"
#include "game_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

struct GameServicePlayerEnterGameNodeCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall>&)>;
AsyncGameServicePlayerEnterGameNodeHandlerFunctionType AsyncGameServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall> call(
        static_cast<AsyncGameServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServicePlayerEnterGameNodeHandler) {
            AsyncGameServicePlayerEnterGameNodeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::PlayerEnterGameNodeRequest& request) {

    AsyncGameServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGameServicePlayerEnterGameNodeGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncPlayerEnterGameNode(&call->context, request,
                                  &registry.get<GameServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceSendMessageToPlayerCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall>&)>;
AsyncGameServiceSendMessageToPlayerHandlerFunctionType AsyncGameServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall> call(
        static_cast<AsyncGameServiceSendMessageToPlayerGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceSendMessageToPlayerHandler) {
            AsyncGameServiceSendMessageToPlayerHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request) {

    AsyncGameServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceSendMessageToPlayerGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSendMessageToPlayer(&call->context, request,
                                  &registry.get<GameServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceClientSendMessageToPlayerCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceClientSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall>&)>;
AsyncGameServiceClientSendMessageToPlayerHandlerFunctionType AsyncGameServiceClientSendMessageToPlayerHandler;

void AsyncCompleteGrpcGameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall> call(
        static_cast<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceClientSendMessageToPlayerHandler) {
            AsyncGameServiceClientSendMessageToPlayerHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::ClientSendMessageToPlayerRequest& request) {

    AsyncGameServiceClientSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceClientSendMessageToPlayerGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncClientSendMessageToPlayer(&call->context, request,
                                  &registry.get<GameServiceClientSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceCentreSendToPlayerViaGameNodeCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceCentreSendToPlayerViaGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall>&)>;
AsyncGameServiceCentreSendToPlayerViaGameNodeHandlerFunctionType AsyncGameServiceCentreSendToPlayerViaGameNodeHandler;

void AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall> call(
        static_cast<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceCentreSendToPlayerViaGameNodeHandler) {
            AsyncGameServiceCentreSendToPlayerViaGameNodeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request) {

    AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall* call = new AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCentreSendToPlayerViaGameNode(&call->context, request,
                                  &registry.get<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceInvokePlayerServiceCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceInvokePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall>&)>;
AsyncGameServiceInvokePlayerServiceHandlerFunctionType AsyncGameServiceInvokePlayerServiceHandler;

void AsyncCompleteGrpcGameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall> call(
        static_cast<AsyncGameServiceInvokePlayerServiceGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceInvokePlayerServiceHandler) {
            AsyncGameServiceInvokePlayerServiceHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request) {

    AsyncGameServiceInvokePlayerServiceGrpcClientCall* call = new AsyncGameServiceInvokePlayerServiceGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncInvokePlayerService(&call->context, request,
                                  &registry.get<GameServiceInvokePlayerServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceRouteNodeStringMsgCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceRouteNodeStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall>&)>;
AsyncGameServiceRouteNodeStringMsgHandlerFunctionType AsyncGameServiceRouteNodeStringMsgHandler;

void AsyncCompleteGrpcGameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall> call(
        static_cast<AsyncGameServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceRouteNodeStringMsgHandler) {
            AsyncGameServiceRouteNodeStringMsgHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request) {

    AsyncGameServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncGameServiceRouteNodeStringMsgGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRouteNodeStringMsg(&call->context, request,
                                  &registry.get<GameServiceRouteNodeStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceRoutePlayerStringMsgCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceRoutePlayerStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall>&)>;
AsyncGameServiceRoutePlayerStringMsgHandlerFunctionType AsyncGameServiceRoutePlayerStringMsgHandler;

void AsyncCompleteGrpcGameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall> call(
        static_cast<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceRoutePlayerStringMsgHandler) {
            AsyncGameServiceRoutePlayerStringMsgHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request) {

    AsyncGameServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncGameServiceRoutePlayerStringMsgGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
                                  &registry.get<GameServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceUpdateSessionDetailCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceUpdateSessionDetailHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall>&)>;
AsyncGameServiceUpdateSessionDetailHandlerFunctionType AsyncGameServiceUpdateSessionDetailHandler;

void AsyncCompleteGrpcGameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall> call(
        static_cast<AsyncGameServiceUpdateSessionDetailGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceUpdateSessionDetailHandler) {
            AsyncGameServiceUpdateSessionDetailHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, const ::RegisterPlayerSessionRequest& request) {

    AsyncGameServiceUpdateSessionDetailGrpcClientCall* call = new AsyncGameServiceUpdateSessionDetailGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncUpdateSessionDetail(&call->context, request,
                                  &registry.get<GameServiceUpdateSessionDetailCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceEnterSceneCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceEnterSceneHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall>&)>;
AsyncGameServiceEnterSceneHandlerFunctionType AsyncGameServiceEnterSceneHandler;

void AsyncCompleteGrpcGameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall> call(
        static_cast<AsyncGameServiceEnterSceneGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceEnterSceneHandler) {
            AsyncGameServiceEnterSceneHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::Centre2GsEnterSceneRequest& request) {

    AsyncGameServiceEnterSceneGrpcClientCall* call = new AsyncGameServiceEnterSceneGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterScene(&call->context, request,
                                  &registry.get<GameServiceEnterSceneCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceCreateSceneCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceCreateSceneHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall>&)>;
AsyncGameServiceCreateSceneHandlerFunctionType AsyncGameServiceCreateSceneHandler;

void AsyncCompleteGrpcGameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall> call(
        static_cast<AsyncGameServiceCreateSceneGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceCreateSceneHandler) {
            AsyncGameServiceCreateSceneHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request) {

    AsyncGameServiceCreateSceneGrpcClientCall* call = new AsyncGameServiceCreateSceneGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCreateScene(&call->context, request,
                                  &registry.get<GameServiceCreateSceneCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GameServiceRegisterNodeSessionCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGameServiceRegisterNodeSessionHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRegisterNodeSessionGrpcClientCall>&)>;
AsyncGameServiceRegisterNodeSessionHandlerFunctionType AsyncGameServiceRegisterNodeSessionHandler;

void AsyncCompleteGrpcGameServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    std::unique_ptr<AsyncGameServiceRegisterNodeSessionGrpcClientCall> call(
        static_cast<AsyncGameServiceRegisterNodeSessionGrpcClientCall*>(got_tag));

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    if (call->status.ok()) {
        if (AsyncGameServiceRegisterNodeSessionHandler) {
            AsyncGameServiceRegisterNodeSessionHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGameServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request) {

    AsyncGameServiceRegisterNodeSessionGrpcClientCall* call = new AsyncGameServiceRegisterNodeSessionGrpcClientCall;
    call->response_reader = registry
        .get<GrpcGameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterNodeSession(&call->context, request,
                                  &registry.get<GameServiceRegisterNodeSessionCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<GameServicePlayerEnterGameNodeCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceSendMessageToPlayerCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceClientSendMessageToPlayerCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceInvokePlayerServiceCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceRouteNodeStringMsgCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceUpdateSessionDetailCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceEnterSceneCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceCreateSceneCompleteQueue>(nodeEntity);

    registry.emplace<GameServiceRegisterNodeSessionCompleteQueue>(nodeEntity);

}
void HandleGameServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<GameServicePlayerEnterGameNodeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServicePlayerEnterGameNode(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceSendMessageToPlayerCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceSendMessageToPlayer(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceClientSendMessageToPlayerCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceClientSendMessageToPlayer(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceInvokePlayerServiceCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceInvokePlayerService(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceRouteNodeStringMsgCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceRouteNodeStringMsg(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceRoutePlayerStringMsgCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceRoutePlayerStringMsg(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceUpdateSessionDetailCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceUpdateSessionDetail(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceEnterSceneCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceEnterScene(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceCreateSceneCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceCreateScene(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GameServiceRegisterNodeSessionCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGameServiceRegisterNodeSession(registry, e, completeQueueComp.cq);
        }
    }
}
