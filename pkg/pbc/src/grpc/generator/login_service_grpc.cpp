#include "muduo/base/Logging.h"

#include "grpc/generator/login_service_grpc.h"
#include "thread_local/storage.h"

using GrpcLoginServiceStubPtr = std::unique_ptr<LoginService::Stub>;
GrpcLoginServiceStubPtr gLoginServiceStub;

entt::entity GlobalGrpcNodeEntity();
struct LoginServiceLoginCompleteQueue{
	grpc::CompletionQueue cq;
};
struct LoginServiceCreatePlayerCompleteQueue{
	grpc::CompletionQueue cq;
};
struct LoginServiceEnterGameCompleteQueue{
	grpc::CompletionQueue cq;
};
struct LoginServiceLeaveGameCompleteQueue{
	grpc::CompletionQueue cq;
};
struct LoginServiceDisconnectCompleteQueue{
	grpc::CompletionQueue cq;
};

void LoginServiceLogin(const LoginC2LRequest& request)
{
    AsyncLoginServiceLoginGrpcClientCall* call = new AsyncLoginServiceLoginGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncLogin(&call->context, request,
		&tls.grpc_node_registry.get<LoginServiceLoginCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)> AsyncLoginServiceLoginHandler;

void AsyncCompleteGrpcLoginServiceLogin()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<LoginServiceLoginCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void LoginServiceCreatePlayer(const CreatePlayerC2LRequest& request)
{
    AsyncLoginServiceCreatePlayerGrpcClientCall* call = new AsyncLoginServiceCreatePlayerGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncCreatePlayer(&call->context, request,
		&tls.grpc_node_registry.get<LoginServiceCreatePlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)> AsyncLoginServiceCreatePlayerHandler;

void AsyncCompleteGrpcLoginServiceCreatePlayer()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<LoginServiceCreatePlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void LoginServiceEnterGame(const EnterGameC2LRequest& request)
{
    AsyncLoginServiceEnterGameGrpcClientCall* call = new AsyncLoginServiceEnterGameGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncEnterGame(&call->context, request,
		&tls.grpc_node_registry.get<LoginServiceEnterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)> AsyncLoginServiceEnterGameHandler;

void AsyncCompleteGrpcLoginServiceEnterGame()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<LoginServiceEnterGameCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void LoginServiceLeaveGame(const LeaveGameC2LRequest& request)
{
    AsyncLoginServiceLeaveGameGrpcClientCall* call = new AsyncLoginServiceLeaveGameGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncLeaveGame(&call->context, request,
		&tls.grpc_node_registry.get<LoginServiceLeaveGameCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)> AsyncLoginServiceLeaveGameHandler;

void AsyncCompleteGrpcLoginServiceLeaveGame()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<LoginServiceLeaveGameCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void LoginServiceDisconnect(const LoginNodeDisconnectRequest& request)
{
    AsyncLoginServiceDisconnectGrpcClientCall* call = new AsyncLoginServiceDisconnectGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncDisconnect(&call->context, request,
		&tls.grpc_node_registry.get<LoginServiceDisconnectCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall>&)> AsyncLoginServiceDisconnectHandler;

void AsyncCompleteGrpcLoginServiceDisconnect()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<LoginServiceDisconnectCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void InitCompletedQueue() {
	tls.grpc_node_registry.emplace<LoginServiceLoginCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<LoginServiceCreatePlayerCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<LoginServiceEnterGameCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<LoginServiceLeaveGameCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<LoginServiceDisconnectCompleteQueue>(GlobalGrpcNodeEntity());
}

void HandleCompletedQueueMessage() {
    AsyncCompleteGrpcLoginServiceLogin();
    AsyncCompleteGrpcLoginServiceCreatePlayer();
    AsyncCompleteGrpcLoginServiceEnterGame();
    AsyncCompleteGrpcLoginServiceLeaveGame();
    AsyncCompleteGrpcLoginServiceDisconnect();
}

