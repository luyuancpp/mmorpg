#include "muduo/base/Logging.h"
#include "game_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"

namespace {
#pragma region GameServicePlayerEnterGameNode
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall> call(
        static_cast<AsyncGameServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncPlayerEnterGameNode(&call->context, request,
                                  &registry.get<GameServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::PlayerEnterGameNodeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGameServicePlayerEnterGameNodeGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncPlayerEnterGameNode(&call->context, request,
                                  &registry.get<GameServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceSendMessageToPlayer
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall> call(
        static_cast<AsyncGameServiceSendMessageToPlayerGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSendMessageToPlayer(&call->context, request,
                                  &registry.get<GameServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceSendMessageToPlayerGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSendMessageToPlayer(&call->context, request,
                                  &registry.get<GameServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceClientSendMessageToPlayer
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall> call(
        static_cast<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncClientSendMessageToPlayer(&call->context, request,
                                  &registry.get<GameServiceClientSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::ClientSendMessageToPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceClientSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceClientSendMessageToPlayerGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncClientSendMessageToPlayer(&call->context, request,
                                  &registry.get<GameServiceClientSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceCentreSendToPlayerViaGameNode
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall> call(
        static_cast<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCentreSendToPlayerViaGameNode(&call->context, request,
                                  &registry.get<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall* call = new AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCentreSendToPlayerViaGameNode(&call->context, request,
                                  &registry.get<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceInvokePlayerService
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall> call(
        static_cast<AsyncGameServiceInvokePlayerServiceGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncInvokePlayerService(&call->context, request,
                                  &registry.get<GameServiceInvokePlayerServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceInvokePlayerServiceGrpcClientCall* call = new AsyncGameServiceInvokePlayerServiceGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncInvokePlayerService(&call->context, request,
                                  &registry.get<GameServiceInvokePlayerServiceCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceRouteNodeStringMsg
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall> call(
        static_cast<AsyncGameServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRouteNodeStringMsg(&call->context, request,
                                  &registry.get<GameServiceRouteNodeStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncGameServiceRouteNodeStringMsgGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRouteNodeStringMsg(&call->context, request,
                                  &registry.get<GameServiceRouteNodeStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceRoutePlayerStringMsg
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall> call(
        static_cast<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
                                  &registry.get<GameServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncGameServiceRoutePlayerStringMsgGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
                                  &registry.get<GameServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceUpdateSessionDetail
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall> call(
        static_cast<AsyncGameServiceUpdateSessionDetailGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncUpdateSessionDetail(&call->context, request,
                                  &registry.get<GameServiceUpdateSessionDetailCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, const ::RegisterPlayerSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceUpdateSessionDetailGrpcClientCall* call = new AsyncGameServiceUpdateSessionDetailGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncUpdateSessionDetail(&call->context, request,
                                  &registry.get<GameServiceUpdateSessionDetailCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceEnterScene
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall> call(
        static_cast<AsyncGameServiceEnterSceneGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterScene(&call->context, request,
                                  &registry.get<GameServiceEnterSceneCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::Centre2GsEnterSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceEnterSceneGrpcClientCall* call = new AsyncGameServiceEnterSceneGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncEnterScene(&call->context, request,
                                  &registry.get<GameServiceEnterSceneCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceCreateScene
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall> call(
        static_cast<AsyncGameServiceCreateSceneGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCreateScene(&call->context, request,
                                  &registry.get<GameServiceCreateSceneCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceCreateSceneGrpcClientCall* call = new AsyncGameServiceCreateSceneGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncCreateScene(&call->context, request,
                                  &registry.get<GameServiceCreateSceneCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



#pragma region GameServiceRegisterNodeSession
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
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncGameServiceRegisterNodeSessionGrpcClientCall> call(
        static_cast<AsyncGameServiceRegisterNodeSessionGrpcClientCall*>(got_tag));
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
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterNodeSession(&call->context, request,
                                  &registry.get<GameServiceRegisterNodeSessionCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGameServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGameServiceRegisterNodeSessionGrpcClientCall* call = new AsyncGameServiceRegisterNodeSessionGrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<GameServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterNodeSession(&call->context, request,
                                  &registry.get<GameServiceRegisterNodeSessionCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
#pragma endregion



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
    }}}// namespace 
