#include "muduo/base/Logging.h"

#include "grpc/generator/db_service_grpc.h"
#include "thread_local/storage.h"
struct AccountDBServiceLoad2RedisCompleteQueue{
	grpc::CompletionQueue cq;
};
struct AccountDBServiceSave2RedisCompleteQueue{
	grpc::CompletionQueue cq;
};

void AccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const  LoadAccountRequest& request)
{
    AsyncAccountDBServiceLoad2RedisGrpcClientCall* call = new AsyncAccountDBServiceLoad2RedisGrpcClientCall;

    call->response_reader =
        registry.get<GrpcAccountDBServiceStubPtr>(nodeEntity)->PrepareAsyncLoad2Redis(&call->context, request,
		&registry.get<AccountDBServiceLoad2RedisCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncAccountDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall>&)>;
AsyncAccountDBServiceLoad2RedisHandlerFunctionType  AsyncAccountDBServiceLoad2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceLoad2Redis(grpc::CompletionQueue& cq)
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

void AccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const  SaveAccountRequest& request)
{
    AsyncAccountDBServiceSave2RedisGrpcClientCall* call = new AsyncAccountDBServiceSave2RedisGrpcClientCall;

    call->response_reader =
        registry.get<GrpcAccountDBServiceStubPtr>(nodeEntity)->PrepareAsyncSave2Redis(&call->context, request,
		&registry.get<AccountDBServiceSave2RedisCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncAccountDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall>&)>;
AsyncAccountDBServiceSave2RedisHandlerFunctionType  AsyncAccountDBServiceSave2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceSave2Redis(grpc::CompletionQueue& cq)
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

void InitAccountDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<AccountDBServiceLoad2RedisCompleteQueue>(nodeEntity);
	registry.emplace<AccountDBServiceSave2RedisCompleteQueue>(nodeEntity);
}

void HandleAccountDBServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<AccountDBServiceLoad2RedisCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcAccountDBServiceLoad2Redis(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<AccountDBServiceSave2RedisCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcAccountDBServiceSave2Redis(completeQueueComp.cq);
		}
	}
}

