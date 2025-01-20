#include "muduo/base/Logging.h"

#include "grpc/generator/gate_service_grpc.h"
#include "thread_local/storage.h"

using GrpcGateServiceStubPtr = std::unique_ptr<GateService::Stub>;
GrpcGateServiceStubPtr gGateServiceStub;

entt::entity GlobalGrpcNodeEntity();
struct GateServiceRegisterGameCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServiceUnRegisterGameCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServicePlayerEnterGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServiceSendMessageToPlayerCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServiceKickSessionByCentreCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServiceRouteNodeMessageCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServiceRoutePlayerMessageCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GateServiceBroadcastToPlayersCompleteQueue{
	grpc::CompletionQueue cq;
};

void GateServiceRegisterGame(const RegisterGameNodeRequest& request)
{
    AsyncGateServiceRegisterGameGrpcClientCall* call = new AsyncGateServiceRegisterGameGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncRegisterGame(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceRegisterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall>&)> AsyncGateServiceRegisterGameHandler;

void AsyncCompleteGrpcGateServiceRegisterGame()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceRegisterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall> call(static_cast<AsyncGateServiceRegisterGameGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceRegisterGameHandler){
			AsyncGateServiceRegisterGameHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServiceUnRegisterGame(const UnregisterGameNodeRequest& request)
{
    AsyncGateServiceUnRegisterGameGrpcClientCall* call = new AsyncGateServiceUnRegisterGameGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncUnRegisterGame(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceUnRegisterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall>&)> AsyncGateServiceUnRegisterGameHandler;

void AsyncCompleteGrpcGateServiceUnRegisterGame()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceUnRegisterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall> call(static_cast<AsyncGateServiceUnRegisterGameGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceUnRegisterGameHandler){
			AsyncGateServiceUnRegisterGameHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServicePlayerEnterGameNode(const RegisterGameNodeSessionRequest& request)
{
    AsyncGateServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGateServicePlayerEnterGameNodeGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncPlayerEnterGameNode(&call->context, request,
		&tls.grpc_node_registry.get<GateServicePlayerEnterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)> AsyncGateServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGateServicePlayerEnterGameNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServicePlayerEnterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall> call(static_cast<AsyncGateServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServicePlayerEnterGameNodeHandler){
			AsyncGateServicePlayerEnterGameNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServiceSendMessageToPlayer(const NodeRouteMessageRequest& request)
{
    AsyncGateServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGateServiceSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncSendMessageToPlayer(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)> AsyncGateServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGateServiceSendMessageToPlayer()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall> call(static_cast<AsyncGateServiceSendMessageToPlayerGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceSendMessageToPlayerHandler){
			AsyncGateServiceSendMessageToPlayerHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServiceKickSessionByCentre(const KickSessionRequest& request)
{
    AsyncGateServiceKickSessionByCentreGrpcClientCall* call = new AsyncGateServiceKickSessionByCentreGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncKickSessionByCentre(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceKickSessionByCentreCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)> AsyncGateServiceKickSessionByCentreHandler;

void AsyncCompleteGrpcGateServiceKickSessionByCentre()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceKickSessionByCentreCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall> call(static_cast<AsyncGateServiceKickSessionByCentreGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceKickSessionByCentreHandler){
			AsyncGateServiceKickSessionByCentreHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServiceRouteNodeMessage(const RouteMessageRequest& request)
{
    AsyncGateServiceRouteNodeMessageGrpcClientCall* call = new AsyncGateServiceRouteNodeMessageGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncRouteNodeMessage(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceRouteNodeMessageCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)> AsyncGateServiceRouteNodeMessageHandler;

void AsyncCompleteGrpcGateServiceRouteNodeMessage()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceRouteNodeMessageCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall> call(static_cast<AsyncGateServiceRouteNodeMessageGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceRouteNodeMessageHandler){
			AsyncGateServiceRouteNodeMessageHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServiceRoutePlayerMessage(const RoutePlayerMessageRequest& request)
{
    AsyncGateServiceRoutePlayerMessageGrpcClientCall* call = new AsyncGateServiceRoutePlayerMessageGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncRoutePlayerMessage(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceRoutePlayerMessageCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)> AsyncGateServiceRoutePlayerMessageHandler;

void AsyncCompleteGrpcGateServiceRoutePlayerMessage()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceRoutePlayerMessageCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall> call(static_cast<AsyncGateServiceRoutePlayerMessageGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceRoutePlayerMessageHandler){
			AsyncGateServiceRoutePlayerMessageHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GateServiceBroadcastToPlayers(const BroadcastToPlayersRequest& request)
{
    AsyncGateServiceBroadcastToPlayersGrpcClientCall* call = new AsyncGateServiceBroadcastToPlayersGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncBroadcastToPlayers(&call->context, request,
		&tls.grpc_node_registry.get<GateServiceBroadcastToPlayersCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall>&)> AsyncGateServiceBroadcastToPlayersHandler;

void AsyncCompleteGrpcGateServiceBroadcastToPlayers()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GateServiceBroadcastToPlayersCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall> call(static_cast<AsyncGateServiceBroadcastToPlayersGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGateServiceBroadcastToPlayersHandler){
			AsyncGateServiceBroadcastToPlayersHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void InitGateServiceCompletedQueue() {
	tls.grpc_node_registry.emplace<GateServiceRegisterGameCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServiceUnRegisterGameCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServicePlayerEnterGameNodeCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServiceSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServiceKickSessionByCentreCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServiceRouteNodeMessageCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServiceRoutePlayerMessageCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GateServiceBroadcastToPlayersCompleteQueue>(GlobalGrpcNodeEntity());
}

void HandleGateServiceCompletedQueueMessage() {
    AsyncCompleteGrpcGateServiceRegisterGame();
    AsyncCompleteGrpcGateServiceUnRegisterGame();
    AsyncCompleteGrpcGateServicePlayerEnterGameNode();
    AsyncCompleteGrpcGateServiceSendMessageToPlayer();
    AsyncCompleteGrpcGateServiceKickSessionByCentre();
    AsyncCompleteGrpcGateServiceRouteNodeMessage();
    AsyncCompleteGrpcGateServiceRoutePlayerMessage();
    AsyncCompleteGrpcGateServiceBroadcastToPlayers();
}

