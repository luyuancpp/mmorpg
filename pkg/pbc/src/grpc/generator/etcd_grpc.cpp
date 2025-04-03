#include "muduo/base/Logging.h"

#include "grpc/generator/etcd_grpc.h"
#include "thread_local/storage.h"
struct etcdserverpbKVRangeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVPutCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVDeleteRangeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVTxnCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVCompactCompleteQueue{
	grpc::CompletionQueue cq;
};

void etcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::RangeRequest& request)
{
    AsyncetcdserverpbKVRangeGrpcClientCall* call = new AsyncetcdserverpbKVRangeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncRange(&call->context, request,
		&registry.get<etcdserverpbKVRangeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>&)>;
AsyncetcdserverpbKVRangeHandlerFunctionType  AsyncetcdserverpbKVRangeHandler;

void AsyncCompleteGrpcetcdserverpbKVRange(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall> call(static_cast<AsyncetcdserverpbKVRangeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVRangeHandler){
			AsyncetcdserverpbKVRangeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void etcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::PutRequest& request)
{
    AsyncetcdserverpbKVPutGrpcClientCall* call = new AsyncetcdserverpbKVPutGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncPut(&call->context, request,
		&registry.get<etcdserverpbKVPutCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>&)>;
AsyncetcdserverpbKVPutHandlerFunctionType  AsyncetcdserverpbKVPutHandler;

void AsyncCompleteGrpcetcdserverpbKVPut(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall> call(static_cast<AsyncetcdserverpbKVPutGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVPutHandler){
			AsyncetcdserverpbKVPutHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void etcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::DeleteRangeRequest& request)
{
    AsyncetcdserverpbKVDeleteRangeGrpcClientCall* call = new AsyncetcdserverpbKVDeleteRangeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncDeleteRange(&call->context, request,
		&registry.get<etcdserverpbKVDeleteRangeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>&)>;
AsyncetcdserverpbKVDeleteRangeHandlerFunctionType  AsyncetcdserverpbKVDeleteRangeHandler;

void AsyncCompleteGrpcetcdserverpbKVDeleteRange(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall> call(static_cast<AsyncetcdserverpbKVDeleteRangeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVDeleteRangeHandler){
			AsyncetcdserverpbKVDeleteRangeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void etcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::TxnRequest& request)
{
    AsyncetcdserverpbKVTxnGrpcClientCall* call = new AsyncetcdserverpbKVTxnGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncTxn(&call->context, request,
		&registry.get<etcdserverpbKVTxnCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>&)>;
AsyncetcdserverpbKVTxnHandlerFunctionType  AsyncetcdserverpbKVTxnHandler;

void AsyncCompleteGrpcetcdserverpbKVTxn(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall> call(static_cast<AsyncetcdserverpbKVTxnGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVTxnHandler){
			AsyncetcdserverpbKVTxnHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void etcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::CompactionRequest& request)
{
    AsyncetcdserverpbKVCompactGrpcClientCall* call = new AsyncetcdserverpbKVCompactGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncCompact(&call->context, request,
		&registry.get<etcdserverpbKVCompactCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVCompactGrpcClientCall>&)>;
AsyncetcdserverpbKVCompactHandlerFunctionType  AsyncetcdserverpbKVCompactHandler;

void AsyncCompleteGrpcetcdserverpbKVCompact(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVCompactGrpcClientCall> call(static_cast<AsyncetcdserverpbKVCompactGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVCompactHandler){
			AsyncetcdserverpbKVCompactHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void InitetcdserverpbKVCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<etcdserverpbKVRangeCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVPutCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVDeleteRangeCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVTxnCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVCompactCompleteQueue>(nodeEntity);
}

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbKVRangeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVRange(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVPutCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVPut(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVDeleteRangeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVDeleteRange(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVTxnCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVTxn(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVCompactCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVCompact(completeQueueComp.cq);
		}
	}
}

