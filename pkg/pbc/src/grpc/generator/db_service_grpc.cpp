#include "muduo/base/Logging.h"

#include "grpc/generator/db_service_grpc.h"
#include "thread_local/storage.h"


entt::entity GlobalGrpcNodeEntity();
struct AccountDBServiceLoad2RedisCompleteQueue{
	grpc::CompletionQueue cq;
};
struct AccountDBServiceSave2RedisCompleteQueue{
	grpc::CompletionQueue cq;
};

void AccountDBServiceLoad2Redis(GrpcAccountDBServiceStubPtr& stub, const LoadAccountRequest& request)
{
    AsyncAccountDBServiceLoad2RedisGrpcClientCall* call = new AsyncAccountDBServiceLoad2RedisGrpcClientCall;

    call->response_reader =
        stub->PrepareAsyncLoad2Redis(&call->context, request,
		&tls.grpc_node_registry.get<AccountDBServiceLoad2RedisCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall>&)> AsyncAccountDBServiceLoad2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceLoad2Redis()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<AccountDBServiceLoad2RedisCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall> call(static_cast<AsyncAccountDBServiceLoad2RedisGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncAccountDBServiceLoad2RedisHandler){
			AsyncAccountDBServiceLoad2RedisHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void AccountDBServiceSave2Redis(GrpcAccountDBServiceStubPtr& stub, const SaveAccountRequest& request)
{
    AsyncAccountDBServiceSave2RedisGrpcClientCall* call = new AsyncAccountDBServiceSave2RedisGrpcClientCall;

    call->response_reader =
        stub->PrepareAsyncSave2Redis(&call->context, request,
		&tls.grpc_node_registry.get<AccountDBServiceSave2RedisCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall>&)> AsyncAccountDBServiceSave2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceSave2Redis()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<AccountDBServiceSave2RedisCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall> call(static_cast<AsyncAccountDBServiceSave2RedisGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncAccountDBServiceSave2RedisHandler){
			AsyncAccountDBServiceSave2RedisHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void InitAccountDBServiceCompletedQueue() {
	tls.grpc_node_registry.emplace<AccountDBServiceLoad2RedisCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<AccountDBServiceSave2RedisCompleteQueue>(GlobalGrpcNodeEntity());
}

void HandleAccountDBServiceCompletedQueueMessage() {
    AsyncCompleteGrpcAccountDBServiceLoad2Redis();
    AsyncCompleteGrpcAccountDBServiceSave2Redis();
}

