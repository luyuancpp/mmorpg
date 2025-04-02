#include "muduo/base/Logging.h"

#include "grpc/generator/etcd_grpc.h"
#include "thread_local/storage.h"
struct KVRangeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct KVPutCompleteQueue{
	grpc::CompletionQueue cq;
};
struct KVDeleteRangeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct KVTxnCompleteQueue{
	grpc::CompletionQueue cq;
};
struct KVCompactCompleteQueue{
	grpc::CompletionQueue cq;
};

void KVRange(entt::registry& registry, entt::entity nodeEntity, const RangeRequest& request)
{
    AsyncKVRangeGrpcClientCall* call = new AsyncKVRangeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcKVStubPtr>(nodeEntity)->PrepareAsyncRange(&call->context, request,
		&registry.get<KVRangeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncKVRangeGrpcClientCall>&)> AsyncKVRangeHandler;

void AsyncCompleteGrpcKVRange(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncKVRangeGrpcClientCall> call(static_cast<AsyncKVRangeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncKVRangeHandler){
			AsyncKVRangeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void KVPut(entt::registry& registry, entt::entity nodeEntity, const PutRequest& request)
{
    AsyncKVPutGrpcClientCall* call = new AsyncKVPutGrpcClientCall;

    call->response_reader =
        registry.get<GrpcKVStubPtr>(nodeEntity)->PrepareAsyncPut(&call->context, request,
		&registry.get<KVPutCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncKVPutGrpcClientCall>&)> AsyncKVPutHandler;

void AsyncCompleteGrpcKVPut(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncKVPutGrpcClientCall> call(static_cast<AsyncKVPutGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncKVPutHandler){
			AsyncKVPutHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void KVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const DeleteRangeRequest& request)
{
    AsyncKVDeleteRangeGrpcClientCall* call = new AsyncKVDeleteRangeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcKVStubPtr>(nodeEntity)->PrepareAsyncDeleteRange(&call->context, request,
		&registry.get<KVDeleteRangeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncKVDeleteRangeGrpcClientCall>&)> AsyncKVDeleteRangeHandler;

void AsyncCompleteGrpcKVDeleteRange(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncKVDeleteRangeGrpcClientCall> call(static_cast<AsyncKVDeleteRangeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncKVDeleteRangeHandler){
			AsyncKVDeleteRangeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void KVTxn(entt::registry& registry, entt::entity nodeEntity, const TxnRequest& request)
{
    AsyncKVTxnGrpcClientCall* call = new AsyncKVTxnGrpcClientCall;

    call->response_reader =
        registry.get<GrpcKVStubPtr>(nodeEntity)->PrepareAsyncTxn(&call->context, request,
		&registry.get<KVTxnCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncKVTxnGrpcClientCall>&)> AsyncKVTxnHandler;

void AsyncCompleteGrpcKVTxn(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncKVTxnGrpcClientCall> call(static_cast<AsyncKVTxnGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncKVTxnHandler){
			AsyncKVTxnHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void KVCompact(entt::registry& registry, entt::entity nodeEntity, const CompactionRequest& request)
{
    AsyncKVCompactGrpcClientCall* call = new AsyncKVCompactGrpcClientCall;

    call->response_reader =
        registry.get<GrpcKVStubPtr>(nodeEntity)->PrepareAsyncCompact(&call->context, request,
		&registry.get<KVCompactCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncKVCompactGrpcClientCall>&)> AsyncKVCompactHandler;

void AsyncCompleteGrpcKVCompact(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncKVCompactGrpcClientCall> call(static_cast<AsyncKVCompactGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncKVCompactHandler){
			AsyncKVCompactHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<KVRangeCompleteQueue>(nodeEntity);
	registry.emplace<KVPutCompleteQueue>(nodeEntity);
	registry.emplace<KVDeleteRangeCompleteQueue>(nodeEntity);
	registry.emplace<KVTxnCompleteQueue>(nodeEntity);
	registry.emplace<KVCompactCompleteQueue>(nodeEntity);
}

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<KVRangeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcKVRange(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<KVPutCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcKVPut(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<KVDeleteRangeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcKVDeleteRange(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<KVTxnCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcKVTxn(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<KVCompactCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcKVCompact(completeQueueComp.cq);
		}
	}
}

