#include "muduo/base/Logging.h"

#include "grpc/generator/centre_service_grpc.h"
#include "thread_local/storage.h"

using GrpcCentreServiceStubPtr = std::unique_ptr<CentreService::Stub>;
GrpcCentreServiceStubPtr gCentreServiceStub;

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

void CentreServiceRegisterGameNode(const RegisterGameNodeRequest& request)
{
    AsyncCentreServiceRegisterGameNodeGrpcClientCall* call = new AsyncCentreServiceRegisterGameNodeGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRegisterGameNode(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceRegisterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterGameNodeGrpcClientCall>&)> AsyncCentreServiceRegisterGameNodeHandler;

void AsyncCompleteGrpcCentreServiceRegisterGameNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceRegisterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceRegisterGateNode(const RegisterGateNodeRequest& request)
{
    AsyncCentreServiceRegisterGateNodeGrpcClientCall* call = new AsyncCentreServiceRegisterGateNodeGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRegisterGateNode(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceRegisterGateNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterGateNodeGrpcClientCall>&)> AsyncCentreServiceRegisterGateNodeHandler;

void AsyncCompleteGrpcCentreServiceRegisterGateNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceRegisterGateNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceGatePlayerService(const GateClientMessageRequest& request)
{
    AsyncCentreServiceGatePlayerServiceGrpcClientCall* call = new AsyncCentreServiceGatePlayerServiceGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncGatePlayerService(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceGatePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall>&)> AsyncCentreServiceGatePlayerServiceHandler;

void AsyncCompleteGrpcCentreServiceGatePlayerService()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceGatePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceGateSessionDisconnect(const GateSessionDisconnectRequest& request)
{
    AsyncCentreServiceGateSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceGateSessionDisconnectGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncGateSessionDisconnect(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceGateSessionDisconnectCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall>&)> AsyncCentreServiceGateSessionDisconnectHandler;

void AsyncCompleteGrpcCentreServiceGateSessionDisconnect()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceGateSessionDisconnectCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceLoginNodeAccountLogin(const LoginRequest& request)
{
    AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall* call = new AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeAccountLogin(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceLoginNodeAccountLoginCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall>&)> AsyncCentreServiceLoginNodeAccountLoginHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceLoginNodeAccountLoginCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceLoginNodeEnterGame(const CentrePlayerGameNodeEntryRequest& request)
{
    AsyncCentreServiceLoginNodeEnterGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeEnterGameGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeEnterGame(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceLoginNodeEnterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall>&)> AsyncCentreServiceLoginNodeEnterGameHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeEnterGame()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceLoginNodeEnterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceLoginNodeLeaveGame(const LoginNodeLeaveGameRequest& request)
{
    AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeLeaveGame(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceLoginNodeLeaveGameCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall>&)> AsyncCentreServiceLoginNodeLeaveGameHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceLoginNodeLeaveGameCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceLoginNodeSessionDisconnect(const GateSessionDisconnectRequest& request)
{
    AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeSessionDisconnect(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceLoginNodeSessionDisconnectCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall>&)> AsyncCentreServiceLoginNodeSessionDisconnectHandler;

void AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceLoginNodeSessionDisconnectCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServicePlayerService(const NodeRouteMessageRequest& request)
{
    AsyncCentreServicePlayerServiceGrpcClientCall* call = new AsyncCentreServicePlayerServiceGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncPlayerService(&call->context, request,
		&tls.grpc_node_registry.get<CentreServicePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall>&)> AsyncCentreServicePlayerServiceHandler;

void AsyncCompleteGrpcCentreServicePlayerService()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServicePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceEnterGsSucceed(const EnterGameNodeSuccessRequest& request)
{
    AsyncCentreServiceEnterGsSucceedGrpcClientCall* call = new AsyncCentreServiceEnterGsSucceedGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncEnterGsSucceed(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceEnterGsSucceedCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall>&)> AsyncCentreServiceEnterGsSucceedHandler;

void AsyncCompleteGrpcCentreServiceEnterGsSucceed()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceEnterGsSucceedCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceRouteNodeStringMsg(const RouteMessageRequest& request)
{
    AsyncCentreServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncCentreServiceRouteNodeStringMsgGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRouteNodeStringMsg(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceRouteNodeStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall>&)> AsyncCentreServiceRouteNodeStringMsgHandler;

void AsyncCompleteGrpcCentreServiceRouteNodeStringMsg()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceRouteNodeStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceRoutePlayerStringMsg(const RoutePlayerMessageRequest& request)
{
    AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceRoutePlayerStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall>&)> AsyncCentreServiceRoutePlayerStringMsgHandler;

void AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceRoutePlayerStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void CentreServiceUnRegisterGameNode(const UnregisterGameNodeRequest& request)
{
    AsyncCentreServiceUnRegisterGameNodeGrpcClientCall* call = new AsyncCentreServiceUnRegisterGameNodeGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncUnRegisterGameNode(&call->context, request,
		&tls.grpc_node_registry.get<CentreServiceUnRegisterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall>&)> AsyncCentreServiceUnRegisterGameNodeHandler;

void AsyncCompleteGrpcCentreServiceUnRegisterGameNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<CentreServiceUnRegisterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void InitCentreServiceCompletedQueue() {
	tls.grpc_node_registry.emplace<CentreServiceRegisterGameNodeCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceRegisterGateNodeCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceGatePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceGateSessionDisconnectCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceLoginNodeAccountLoginCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceLoginNodeEnterGameCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceLoginNodeLeaveGameCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceLoginNodeSessionDisconnectCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServicePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceEnterGsSucceedCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceRouteNodeStringMsgCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceRoutePlayerStringMsgCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<CentreServiceUnRegisterGameNodeCompleteQueue>(GlobalGrpcNodeEntity());
}

void HandleCentreServiceCompletedQueueMessage() {
    AsyncCompleteGrpcCentreServiceRegisterGameNode();
    AsyncCompleteGrpcCentreServiceRegisterGateNode();
    AsyncCompleteGrpcCentreServiceGatePlayerService();
    AsyncCompleteGrpcCentreServiceGateSessionDisconnect();
    AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin();
    AsyncCompleteGrpcCentreServiceLoginNodeEnterGame();
    AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame();
    AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect();
    AsyncCompleteGrpcCentreServicePlayerService();
    AsyncCompleteGrpcCentreServiceEnterGsSucceed();
    AsyncCompleteGrpcCentreServiceRouteNodeStringMsg();
    AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg();
    AsyncCompleteGrpcCentreServiceUnRegisterGameNode();
}

