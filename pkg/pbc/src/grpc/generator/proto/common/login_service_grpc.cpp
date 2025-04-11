#include "muduo/base/Logging.h"

#include "login_service_grpc.h"
#include "thread_local/storage.h"
struct LoginServiceLoginCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const  ::LoginC2LRequest& request)
{
    AsyncLoginServiceLoginGrpcClientCall* call = new AsyncLoginServiceLoginGrpcClientCall;


    call->response_reader =
        registry.get<GrpcLoginServiceStubPtr>(nodeEntity)->PrepareAsyncLogin(&call->context, request,
		&registry.get<LoginServiceLoginCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)>;
AsyncLoginServiceLoginHandlerFunctionType  AsyncLoginServiceLoginHandler;

void AsyncCompleteGrpcLoginServiceLogin(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall> call(static_cast<AsyncLoginServiceLoginGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncLoginServiceLoginHandler){
			AsyncLoginServiceLoginHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct LoginServiceCreatePlayerCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const  ::CreatePlayerC2LRequest& request)
{
    AsyncLoginServiceCreatePlayerGrpcClientCall* call = new AsyncLoginServiceCreatePlayerGrpcClientCall;


    call->response_reader =
        registry.get<GrpcLoginServiceStubPtr>(nodeEntity)->PrepareAsyncCreatePlayer(&call->context, request,
		&registry.get<LoginServiceCreatePlayerCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)>;
AsyncLoginServiceCreatePlayerHandlerFunctionType  AsyncLoginServiceCreatePlayerHandler;

void AsyncCompleteGrpcLoginServiceCreatePlayer(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall> call(static_cast<AsyncLoginServiceCreatePlayerGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncLoginServiceCreatePlayerHandler){
			AsyncLoginServiceCreatePlayerHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct LoginServiceEnterGameCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const  ::EnterGameC2LRequest& request)
{
    AsyncLoginServiceEnterGameGrpcClientCall* call = new AsyncLoginServiceEnterGameGrpcClientCall;


    call->response_reader =
        registry.get<GrpcLoginServiceStubPtr>(nodeEntity)->PrepareAsyncEnterGame(&call->context, request,
		&registry.get<LoginServiceEnterGameCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)>;
AsyncLoginServiceEnterGameHandlerFunctionType  AsyncLoginServiceEnterGameHandler;

void AsyncCompleteGrpcLoginServiceEnterGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall> call(static_cast<AsyncLoginServiceEnterGameGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncLoginServiceEnterGameHandler){
			AsyncLoginServiceEnterGameHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct LoginServiceLeaveGameCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const  ::LeaveGameC2LRequest& request)
{
    AsyncLoginServiceLeaveGameGrpcClientCall* call = new AsyncLoginServiceLeaveGameGrpcClientCall;


    call->response_reader =
        registry.get<GrpcLoginServiceStubPtr>(nodeEntity)->PrepareAsyncLeaveGame(&call->context, request,
		&registry.get<LoginServiceLeaveGameCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)>;
AsyncLoginServiceLeaveGameHandlerFunctionType  AsyncLoginServiceLeaveGameHandler;

void AsyncCompleteGrpcLoginServiceLeaveGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall> call(static_cast<AsyncLoginServiceLeaveGameGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncLoginServiceLeaveGameHandler){
			AsyncLoginServiceLeaveGameHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct LoginServiceDisconnectCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const  ::LoginNodeDisconnectRequest& request)
{
    AsyncLoginServiceDisconnectGrpcClientCall* call = new AsyncLoginServiceDisconnectGrpcClientCall;


    call->response_reader =
        registry.get<GrpcLoginServiceStubPtr>(nodeEntity)->PrepareAsyncDisconnect(&call->context, request,
		&registry.get<LoginServiceDisconnectCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall>&)>;
AsyncLoginServiceDisconnectHandlerFunctionType  AsyncLoginServiceDisconnectHandler;

void AsyncCompleteGrpcLoginServiceDisconnect(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall> call(static_cast<AsyncLoginServiceDisconnectGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncLoginServiceDisconnectHandler){
			AsyncLoginServiceDisconnectHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<LoginServiceLoginCompleteQueue>(nodeEntity);
	registry.emplace<LoginServiceCreatePlayerCompleteQueue>(nodeEntity);
	registry.emplace<LoginServiceEnterGameCompleteQueue>(nodeEntity);
	registry.emplace<LoginServiceLeaveGameCompleteQueue>(nodeEntity);
	registry.emplace<LoginServiceDisconnectCompleteQueue>(nodeEntity);
}
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<LoginServiceLoginCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcLoginServiceLogin(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<LoginServiceCreatePlayerCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcLoginServiceCreatePlayer(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<LoginServiceEnterGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcLoginServiceEnterGame(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<LoginServiceLeaveGameCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcLoginServiceLeaveGame(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<LoginServiceDisconnectCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcLoginServiceDisconnect(completeQueueComp.cq);
		}
	}
}
