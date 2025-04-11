#include "muduo/base/Logging.h"

#include "gate_service_grpc.h"
#include "thread_local/storage.h"
struct GateServiceRegisterGameCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeRequest& request)
{

    AsyncGateServiceRegisterGameGrpcClientCall* call = new AsyncGateServiceRegisterGameGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncRegisterGame(&call->context, request,
		&registry.get<GateServiceRegisterGameCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall>&)>;
AsyncGateServiceRegisterGameHandlerFunctionType  AsyncGateServiceRegisterGameHandler;

void AsyncCompleteGrpcGateServiceRegisterGame(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServiceUnRegisterGameCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceUnRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::UnregisterGameNodeRequest& request)
{

    AsyncGateServiceUnRegisterGameGrpcClientCall* call = new AsyncGateServiceUnRegisterGameGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncUnRegisterGame(&call->context, request,
		&registry.get<GateServiceUnRegisterGameCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceUnRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall>&)>;
AsyncGateServiceUnRegisterGameHandlerFunctionType  AsyncGateServiceUnRegisterGameHandler;

void AsyncCompleteGrpcGateServiceUnRegisterGame(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServicePlayerEnterGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeSessionRequest& request)
{

    AsyncGateServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGateServicePlayerEnterGameNodeGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncPlayerEnterGameNode(&call->context, request,
		&registry.get<GateServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)>;
AsyncGateServicePlayerEnterGameNodeHandlerFunctionType  AsyncGateServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGateServicePlayerEnterGameNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServiceSendMessageToPlayerCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request)
{

    AsyncGateServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGateServiceSendMessageToPlayerGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncSendMessageToPlayer(&call->context, request,
		&registry.get<GateServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)>;
AsyncGateServiceSendMessageToPlayerHandlerFunctionType  AsyncGateServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGateServiceSendMessageToPlayer(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServiceKickSessionByCentreCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const  ::KickSessionRequest& request)
{

    AsyncGateServiceKickSessionByCentreGrpcClientCall* call = new AsyncGateServiceKickSessionByCentreGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncKickSessionByCentre(&call->context, request,
		&registry.get<GateServiceKickSessionByCentreCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceKickSessionByCentreHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)>;
AsyncGateServiceKickSessionByCentreHandlerFunctionType  AsyncGateServiceKickSessionByCentreHandler;

void AsyncCompleteGrpcGateServiceKickSessionByCentre(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServiceRouteNodeMessageCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RouteMessageRequest& request)
{

    AsyncGateServiceRouteNodeMessageGrpcClientCall* call = new AsyncGateServiceRouteNodeMessageGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncRouteNodeMessage(&call->context, request,
		&registry.get<GateServiceRouteNodeMessageCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceRouteNodeMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)>;
AsyncGateServiceRouteNodeMessageHandlerFunctionType  AsyncGateServiceRouteNodeMessageHandler;

void AsyncCompleteGrpcGateServiceRouteNodeMessage(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServiceRoutePlayerMessageCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RoutePlayerMessageRequest& request)
{

    AsyncGateServiceRoutePlayerMessageGrpcClientCall* call = new AsyncGateServiceRoutePlayerMessageGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncRoutePlayerMessage(&call->context, request,
		&registry.get<GateServiceRoutePlayerMessageCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceRoutePlayerMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)>;
AsyncGateServiceRoutePlayerMessageHandlerFunctionType  AsyncGateServiceRoutePlayerMessageHandler;

void AsyncCompleteGrpcGateServiceRoutePlayerMessage(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

struct GateServiceBroadcastToPlayersCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const  ::BroadcastToPlayersRequest& request)
{

    AsyncGateServiceBroadcastToPlayersGrpcClientCall* call = new AsyncGateServiceBroadcastToPlayersGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncBroadcastToPlayers(&call->context, request,
		&registry.get<GateServiceBroadcastToPlayersCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}


using AsyncGateServiceBroadcastToPlayersHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall>&)>;
AsyncGateServiceBroadcastToPlayersHandlerFunctionType  AsyncGateServiceBroadcastToPlayersHandler;

void AsyncCompleteGrpcGateServiceBroadcastToPlayers(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<GateServiceRegisterGameCompleteQueue>(nodeEntity);

	registry.emplace<GateServiceUnRegisterGameCompleteQueue>(nodeEntity);

	registry.emplace<GateServicePlayerEnterGameNodeCompleteQueue>(nodeEntity);

	registry.emplace<GateServiceSendMessageToPlayerCompleteQueue>(nodeEntity);

	registry.emplace<GateServiceKickSessionByCentreCompleteQueue>(nodeEntity);

	registry.emplace<GateServiceRouteNodeMessageCompleteQueue>(nodeEntity);

	registry.emplace<GateServiceRoutePlayerMessageCompleteQueue>(nodeEntity);

	registry.emplace<GateServiceBroadcastToPlayersCompleteQueue>(nodeEntity);

}
void HandleGateServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<GateServiceRegisterGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceRegisterGame(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceUnRegisterGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceUnRegisterGame(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServicePlayerEnterGameNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServicePlayerEnterGameNode(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceSendMessageToPlayerCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceSendMessageToPlayer(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceKickSessionByCentreCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceKickSessionByCentre(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceRouteNodeMessageCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceRouteNodeMessage(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceRoutePlayerMessageCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceRoutePlayerMessage(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceBroadcastToPlayersCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceBroadcastToPlayers(completeQueueComp.cq);
		}
	}
}
