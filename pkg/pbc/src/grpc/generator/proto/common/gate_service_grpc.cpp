#include "muduo/base/Logging.h"
#include "gate_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

namespace {

struct GateServicePlayerEnterGameNodeCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)>;
AsyncGateServicePlayerEnterGameNodeHandlerFunctionType AsyncGateServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall> call(
        static_cast<AsyncGateServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServicePlayerEnterGameNodeHandler) {
            AsyncGateServicePlayerEnterGameNodeHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::RegisterGameNodeSessionRequest& request) {

    AsyncGateServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGateServicePlayerEnterGameNodeGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncPlayerEnterGameNode(&call->context, request,
                                  &registry.get<GateServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::RegisterGameNodeSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGateServicePlayerEnterGameNodeGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncPlayerEnterGameNode(&call->context, request,
                                  &registry.get<GateServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceSendMessageToPlayerCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)>;
AsyncGateServiceSendMessageToPlayerHandlerFunctionType AsyncGateServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall> call(
        static_cast<AsyncGateServiceSendMessageToPlayerGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServiceSendMessageToPlayerHandler) {
            AsyncGateServiceSendMessageToPlayerHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request) {

    AsyncGateServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGateServiceSendMessageToPlayerGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSendMessageToPlayer(&call->context, request,
                                  &registry.get<GateServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGateServiceSendMessageToPlayerGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSendMessageToPlayer(&call->context, request,
                                  &registry.get<GateServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceKickSessionByCentreCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServiceKickSessionByCentreHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)>;
AsyncGateServiceKickSessionByCentreHandlerFunctionType AsyncGateServiceKickSessionByCentreHandler;

void AsyncCompleteGrpcGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall> call(
        static_cast<AsyncGateServiceKickSessionByCentreGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServiceKickSessionByCentreHandler) {
            AsyncGateServiceKickSessionByCentreHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const ::KickSessionRequest& request) {

    AsyncGateServiceKickSessionByCentreGrpcClientCall* call = new AsyncGateServiceKickSessionByCentreGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncKickSessionByCentre(&call->context, request,
                                  &registry.get<GateServiceKickSessionByCentreCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const ::KickSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServiceKickSessionByCentreGrpcClientCall* call = new AsyncGateServiceKickSessionByCentreGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncKickSessionByCentre(&call->context, request,
                                  &registry.get<GateServiceKickSessionByCentreCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceRouteNodeMessageCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServiceRouteNodeMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)>;
AsyncGateServiceRouteNodeMessageHandlerFunctionType AsyncGateServiceRouteNodeMessageHandler;

void AsyncCompleteGrpcGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall> call(
        static_cast<AsyncGateServiceRouteNodeMessageGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServiceRouteNodeMessageHandler) {
            AsyncGateServiceRouteNodeMessageHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request) {

    AsyncGateServiceRouteNodeMessageGrpcClientCall* call = new AsyncGateServiceRouteNodeMessageGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRouteNodeMessage(&call->context, request,
                                  &registry.get<GateServiceRouteNodeMessageCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServiceRouteNodeMessageGrpcClientCall* call = new AsyncGateServiceRouteNodeMessageGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRouteNodeMessage(&call->context, request,
                                  &registry.get<GateServiceRouteNodeMessageCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceRoutePlayerMessageCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServiceRoutePlayerMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)>;
AsyncGateServiceRoutePlayerMessageHandlerFunctionType AsyncGateServiceRoutePlayerMessageHandler;

void AsyncCompleteGrpcGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall> call(
        static_cast<AsyncGateServiceRoutePlayerMessageGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServiceRoutePlayerMessageHandler) {
            AsyncGateServiceRoutePlayerMessageHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request) {

    AsyncGateServiceRoutePlayerMessageGrpcClientCall* call = new AsyncGateServiceRoutePlayerMessageGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRoutePlayerMessage(&call->context, request,
                                  &registry.get<GateServiceRoutePlayerMessageCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServiceRoutePlayerMessageGrpcClientCall* call = new AsyncGateServiceRoutePlayerMessageGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRoutePlayerMessage(&call->context, request,
                                  &registry.get<GateServiceRoutePlayerMessageCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceBroadcastToPlayersCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServiceBroadcastToPlayersHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall>&)>;
AsyncGateServiceBroadcastToPlayersHandlerFunctionType AsyncGateServiceBroadcastToPlayersHandler;

void AsyncCompleteGrpcGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall> call(
        static_cast<AsyncGateServiceBroadcastToPlayersGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServiceBroadcastToPlayersHandler) {
            AsyncGateServiceBroadcastToPlayersHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const ::BroadcastToPlayersRequest& request) {

    AsyncGateServiceBroadcastToPlayersGrpcClientCall* call = new AsyncGateServiceBroadcastToPlayersGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncBroadcastToPlayers(&call->context, request,
                                  &registry.get<GateServiceBroadcastToPlayersCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const ::BroadcastToPlayersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServiceBroadcastToPlayersGrpcClientCall* call = new AsyncGateServiceBroadcastToPlayersGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncBroadcastToPlayers(&call->context, request,
                                  &registry.get<GateServiceBroadcastToPlayersCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceRegisterNodeSessionCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncGateServiceRegisterNodeSessionHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRegisterNodeSessionGrpcClientCall>&)>;
AsyncGateServiceRegisterNodeSessionHandlerFunctionType AsyncGateServiceRegisterNodeSessionHandler;

void AsyncCompleteGrpcGateServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
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

    std::unique_ptr<AsyncGateServiceRegisterNodeSessionGrpcClientCall> call(
        static_cast<AsyncGateServiceRegisterNodeSessionGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncGateServiceRegisterNodeSessionHandler) {
            AsyncGateServiceRegisterNodeSessionHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendGateServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request) {

    AsyncGateServiceRegisterNodeSessionGrpcClientCall* call = new AsyncGateServiceRegisterNodeSessionGrpcClientCall;
    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterNodeSession(&call->context, request,
                                  &registry.get<GateServiceRegisterNodeSessionCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendGateServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncGateServiceRegisterNodeSessionGrpcClientCall* call = new AsyncGateServiceRegisterNodeSessionGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<GateServiceStubPtr>(nodeEntity)
        ->PrepareAsyncRegisterNodeSession(&call->context, request,
                                  &registry.get<GateServiceRegisterNodeSessionCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<GateServicePlayerEnterGameNodeCompleteQueue>(nodeEntity);

    registry.emplace<GateServiceSendMessageToPlayerCompleteQueue>(nodeEntity);

    registry.emplace<GateServiceKickSessionByCentreCompleteQueue>(nodeEntity);

    registry.emplace<GateServiceRouteNodeMessageCompleteQueue>(nodeEntity);

    registry.emplace<GateServiceRoutePlayerMessageCompleteQueue>(nodeEntity);

    registry.emplace<GateServiceBroadcastToPlayersCompleteQueue>(nodeEntity);

    registry.emplace<GateServiceRegisterNodeSessionCompleteQueue>(nodeEntity);

}
void HandleGateServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<GateServicePlayerEnterGameNodeCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServicePlayerEnterGameNode(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GateServiceSendMessageToPlayerCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServiceSendMessageToPlayer(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GateServiceKickSessionByCentreCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServiceKickSessionByCentre(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GateServiceRouteNodeMessageCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServiceRouteNodeMessage(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GateServiceRoutePlayerMessageCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServiceRoutePlayerMessage(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GateServiceBroadcastToPlayersCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServiceBroadcastToPlayers(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<GateServiceRegisterNodeSessionCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcGateServiceRegisterNodeSession(registry, e, completeQueueComp.cq);
        }
    }
}

}// namespace 
