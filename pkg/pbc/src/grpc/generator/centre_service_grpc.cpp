#include "muduo/base/Logging.h"

#include "grpc/generator/centre_service_grpc.h"
#include "thread_local/storage.h"


entt::entity GlobalGrpcNodeEntity();
struct CentreServiceRegisterGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceRegisterGateNodeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceGatePlayerServiceCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceGateSessionDisconnectCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceLoginNodeAccountLoginCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceLoginNodeEnterGameCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceLoginNodeLeaveGameCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceLoginNodeSessionDisconnectCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServicePlayerServiceCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceEnterGsSucceedCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceRouteNodeStringMsgCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceRoutePlayerStringMsgCompleteQueue{
	grpc::CompletionQueue cq;
};
struct CentreServiceUnRegisterGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};

void CentreServiceRegisterGameNode(entt::registry& registry, entt::entity nodeEntity, const RegisterGameNodeRequest& request)
{
    AsyncCentreServiceRegisterGameNodeGrpcClientCall* call = new AsyncCentreServiceRegisterGameNodeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncRegisterGameNode(&call->context, request,
		&registry.get<CentreServiceRegisterGameNodeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterGameNodeGrpcClientCall>&)> AsyncCentreServiceRegisterGameNodeHandler;

void AsyncCompleteGrpcCentreServiceRegisterGameNode(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRegisterGameNodeGrpcClientCall> call(static_cast<AsyncCentreServiceRegisterGameNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceRegisterGameNodeHandler){
			AsyncCentreServiceRegisterGameNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceRegisterGateNode(entt::registry& registry, entt::entity nodeEntity, const RegisterGateNodeRequest& request)
{
    AsyncCentreServiceRegisterGateNodeGrpcClientCall* call = new AsyncCentreServiceRegisterGateNodeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncRegisterGateNode(&call->context, request,
		&registry.get<CentreServiceRegisterGateNodeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterGateNodeGrpcClientCall>&)> AsyncCentreServiceRegisterGateNodeHandler;

void AsyncCompleteGrpcCentreServiceRegisterGateNode(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRegisterGateNodeGrpcClientCall> call(static_cast<AsyncCentreServiceRegisterGateNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceRegisterGateNodeHandler){
			AsyncCentreServiceRegisterGateNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceGatePlayerService(entt::registry& registry, entt::entity nodeEntity, const GateClientMessageRequest& request)
{
    AsyncCentreServiceGatePlayerServiceGrpcClientCall* call = new AsyncCentreServiceGatePlayerServiceGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncGatePlayerService(&call->context, request,
		&registry.get<CentreServiceGatePlayerServiceCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall>&)> AsyncCentreServiceGatePlayerServiceHandler;

void AsyncCompleteGrpcCentreServiceGatePlayerService(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall> call(static_cast<AsyncCentreServiceGatePlayerServiceGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceGatePlayerServiceHandler){
			AsyncCentreServiceGatePlayerServiceHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceGateSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const GateSessionDisconnectRequest& request)
{
    AsyncCentreServiceGateSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceGateSessionDisconnectGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncGateSessionDisconnect(&call->context, request,
		&registry.get<CentreServiceGateSessionDisconnectCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall>&)> AsyncCentreServiceGateSessionDisconnectHandler;

void AsyncCompleteGrpcCentreServiceGateSessionDisconnect(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall> call(static_cast<AsyncCentreServiceGateSessionDisconnectGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceGateSessionDisconnectHandler){
			AsyncCentreServiceGateSessionDisconnectHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceLoginNodeAccountLogin(entt::registry& registry, entt::entity nodeEntity, const LoginRequest& request)
{
    AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall* call = new AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncLoginNodeAccountLogin(&call->context, request,
		&registry.get<CentreServiceLoginNodeAccountLoginCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall>&)> AsyncCentreServiceLoginNodeAccountLoginHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceLoginNodeAccountLoginHandler){
			AsyncCentreServiceLoginNodeAccountLoginHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceLoginNodeEnterGame(entt::registry& registry, entt::entity nodeEntity, const CentrePlayerGameNodeEntryRequest& request)
{
    AsyncCentreServiceLoginNodeEnterGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeEnterGameGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncLoginNodeEnterGame(&call->context, request,
		&registry.get<CentreServiceLoginNodeEnterGameCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall>&)> AsyncCentreServiceLoginNodeEnterGameHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeEnterGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceLoginNodeEnterGameHandler){
			AsyncCentreServiceLoginNodeEnterGameHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceLoginNodeLeaveGame(entt::registry& registry, entt::entity nodeEntity, const LoginNodeLeaveGameRequest& request)
{
    AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncLoginNodeLeaveGame(&call->context, request,
		&registry.get<CentreServiceLoginNodeLeaveGameCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall>&)> AsyncCentreServiceLoginNodeLeaveGameHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceLoginNodeLeaveGameHandler){
			AsyncCentreServiceLoginNodeLeaveGameHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceLoginNodeSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const GateSessionDisconnectRequest& request)
{
    AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncLoginNodeSessionDisconnect(&call->context, request,
		&registry.get<CentreServiceLoginNodeSessionDisconnectCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall>&)> AsyncCentreServiceLoginNodeSessionDisconnectHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceLoginNodeSessionDisconnectHandler){
			AsyncCentreServiceLoginNodeSessionDisconnectHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServicePlayerService(entt::registry& registry, entt::entity nodeEntity, const NodeRouteMessageRequest& request)
{
    AsyncCentreServicePlayerServiceGrpcClientCall* call = new AsyncCentreServicePlayerServiceGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncPlayerService(&call->context, request,
		&registry.get<CentreServicePlayerServiceCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall>&)> AsyncCentreServicePlayerServiceHandler;

void AsyncCompleteGrpcCentreServicePlayerService(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall> call(static_cast<AsyncCentreServicePlayerServiceGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServicePlayerServiceHandler){
			AsyncCentreServicePlayerServiceHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceEnterGsSucceed(entt::registry& registry, entt::entity nodeEntity, const EnterGameNodeSuccessRequest& request)
{
    AsyncCentreServiceEnterGsSucceedGrpcClientCall* call = new AsyncCentreServiceEnterGsSucceedGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncEnterGsSucceed(&call->context, request,
		&registry.get<CentreServiceEnterGsSucceedCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall>&)> AsyncCentreServiceEnterGsSucceedHandler;

void AsyncCompleteGrpcCentreServiceEnterGsSucceed(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall> call(static_cast<AsyncCentreServiceEnterGsSucceedGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceEnterGsSucceedHandler){
			AsyncCentreServiceEnterGsSucceedHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const RouteMessageRequest& request)
{
    AsyncCentreServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncCentreServiceRouteNodeStringMsgGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncRouteNodeStringMsg(&call->context, request,
		&registry.get<CentreServiceRouteNodeStringMsgCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall>&)> AsyncCentreServiceRouteNodeStringMsgHandler;

void AsyncCompleteGrpcCentreServiceRouteNodeStringMsg(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall> call(static_cast<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceRouteNodeStringMsgHandler){
			AsyncCentreServiceRouteNodeStringMsgHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const RoutePlayerMessageRequest& request)
{
    AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
		&registry.get<CentreServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall>&)> AsyncCentreServiceRoutePlayerStringMsgHandler;

void AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall> call(static_cast<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceRoutePlayerStringMsgHandler){
			AsyncCentreServiceRoutePlayerStringMsgHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void CentreServiceUnRegisterGameNode(entt::registry& registry, entt::entity nodeEntity, const UnregisterGameNodeRequest& request)
{
    AsyncCentreServiceUnRegisterGameNodeGrpcClientCall* call = new AsyncCentreServiceUnRegisterGameNodeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcCentreServiceStubPtr>(nodeEntity)->PrepareAsyncUnRegisterGameNode(&call->context, request,
		&registry.get<CentreServiceUnRegisterGameNodeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall>&)> AsyncCentreServiceUnRegisterGameNodeHandler;

void AsyncCompleteGrpcCentreServiceUnRegisterGameNode(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall> call(static_cast<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncCentreServiceUnRegisterGameNodeHandler){
			AsyncCentreServiceUnRegisterGameNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<CentreServiceRegisterGameNodeCompleteQueue>(nodeEntity);
	registry.emplace<CentreServiceRegisterGateNodeCompleteQueue>(nodeEntity);
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
	registry.emplace<CentreServiceUnRegisterGameNodeCompleteQueue>(nodeEntity);
}

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<CentreServiceRegisterGameNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceRegisterGameNode(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceRegisterGateNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceRegisterGateNode(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceGatePlayerServiceCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceGatePlayerService(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceGateSessionDisconnectCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceGateSessionDisconnect(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceLoginNodeAccountLoginCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceLoginNodeEnterGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceLoginNodeEnterGame(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceLoginNodeLeaveGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceLoginNodeSessionDisconnectCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServicePlayerServiceCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServicePlayerService(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceEnterGsSucceedCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceEnterGsSucceed(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceRouteNodeStringMsgCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceRouteNodeStringMsg(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceRoutePlayerStringMsgCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<CentreServiceUnRegisterGameNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcCentreServiceUnRegisterGameNode(completeQueueComp.cq);
		}
	}
}

