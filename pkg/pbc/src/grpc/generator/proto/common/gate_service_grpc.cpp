#include "muduo/base/Logging.h"

#include "gate_service_grpc.h"
#include "thread_local/storage.h"

  enum class GrpcTag {
        INIT = 1,
        WRITE,
        READ,
        WRITES_DONE,
        FINISH
    };

struct GateServiceRegisterGameCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall>&)>;
AsyncGateServiceRegisterGameHandlerFunctionType  AsyncGateServiceRegisterGameHandler;

void AsyncCompleteGrpcGateServiceRegisterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeRequest& request)
{

    AsyncGateServiceRegisterGameGrpcClientCall* call = new AsyncGateServiceRegisterGameGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncRegisterGame(&call->context, request,
		&registry.get<GateServiceRegisterGameCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceUnRegisterGameCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceUnRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall>&)>;
AsyncGateServiceUnRegisterGameHandlerFunctionType  AsyncGateServiceUnRegisterGameHandler;

void AsyncCompleteGrpcGateServiceUnRegisterGame(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceUnRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::UnregisterGameNodeRequest& request)
{

    AsyncGateServiceUnRegisterGameGrpcClientCall* call = new AsyncGateServiceUnRegisterGameGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncUnRegisterGame(&call->context, request,
		&registry.get<GateServiceUnRegisterGameCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServicePlayerEnterGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)>;
AsyncGateServicePlayerEnterGameNodeHandlerFunctionType  AsyncGateServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeSessionRequest& request)
{

    AsyncGateServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGateServicePlayerEnterGameNodeGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncPlayerEnterGameNode(&call->context, request,
		&registry.get<GateServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceSendMessageToPlayerCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)>;
AsyncGateServiceSendMessageToPlayerHandlerFunctionType  AsyncGateServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request)
{

    AsyncGateServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGateServiceSendMessageToPlayerGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncSendMessageToPlayer(&call->context, request,
		&registry.get<GateServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceKickSessionByCentreCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceKickSessionByCentreHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)>;
AsyncGateServiceKickSessionByCentreHandlerFunctionType  AsyncGateServiceKickSessionByCentreHandler;

void AsyncCompleteGrpcGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const  ::KickSessionRequest& request)
{

    AsyncGateServiceKickSessionByCentreGrpcClientCall* call = new AsyncGateServiceKickSessionByCentreGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncKickSessionByCentre(&call->context, request,
		&registry.get<GateServiceKickSessionByCentreCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceRouteNodeMessageCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceRouteNodeMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)>;
AsyncGateServiceRouteNodeMessageHandlerFunctionType  AsyncGateServiceRouteNodeMessageHandler;

void AsyncCompleteGrpcGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RouteMessageRequest& request)
{

    AsyncGateServiceRouteNodeMessageGrpcClientCall* call = new AsyncGateServiceRouteNodeMessageGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncRouteNodeMessage(&call->context, request,
		&registry.get<GateServiceRouteNodeMessageCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceRoutePlayerMessageCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceRoutePlayerMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)>;
AsyncGateServiceRoutePlayerMessageHandlerFunctionType  AsyncGateServiceRoutePlayerMessageHandler;

void AsyncCompleteGrpcGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RoutePlayerMessageRequest& request)
{

    AsyncGateServiceRoutePlayerMessageGrpcClientCall* call = new AsyncGateServiceRoutePlayerMessageGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncRoutePlayerMessage(&call->context, request,
		&registry.get<GateServiceRoutePlayerMessageCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct GateServiceBroadcastToPlayersCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncGateServiceBroadcastToPlayersHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall>&)>;
AsyncGateServiceBroadcastToPlayersHandlerFunctionType  AsyncGateServiceBroadcastToPlayersHandler;

void AsyncCompleteGrpcGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const  ::BroadcastToPlayersRequest& request)
{

    AsyncGateServiceBroadcastToPlayersGrpcClientCall* call = new AsyncGateServiceBroadcastToPlayersGrpcClientCall;
    call->response_reader =
        registry.get<GrpcGateServiceStubPtr>(nodeEntity)->PrepareAsyncBroadcastToPlayers(&call->context, request,
		&registry.get<GateServiceBroadcastToPlayersCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

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
			AsyncCompleteGrpcGateServiceRegisterGame(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceUnRegisterGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceUnRegisterGame(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServicePlayerEnterGameNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServicePlayerEnterGameNode(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceSendMessageToPlayerCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceSendMessageToPlayer(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceKickSessionByCentreCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceKickSessionByCentre(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceRouteNodeMessageCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceRouteNodeMessage(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceRoutePlayerMessageCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceRoutePlayerMessage(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GateServiceBroadcastToPlayersCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGateServiceBroadcastToPlayers(registry, e, completeQueueComp.cq);
		}
	}
}
