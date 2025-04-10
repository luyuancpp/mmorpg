#include "muduo/base/Logging.h"

#include "grpc/generator/etcd_grpc.h"
#include "thread_local/storage.h"
struct etcdserverpbKVLeaseGrantCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVLeaseRevokeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVLeaseKeepAliveCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVLeaseTimeToLiveCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVLeaseLeasesCompleteQueue{
	grpc::CompletionQueue cq;
};
struct etcdserverpbKVWatchCompleteQueue{
	grpc::CompletionQueue cq;
};
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

void SendetcdserverpbKVLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseGrantRequest& request)
{
    AsyncetcdserverpbKVLeaseGrantGrpcClientCall* call = new AsyncetcdserverpbKVLeaseGrantGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncLeaseGrant(&call->context, request,
		&registry.get<etcdserverpbKVLeaseGrantCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseGrantGrpcClientCall>&)>;
AsyncetcdserverpbKVLeaseGrantHandlerFunctionType  AsyncetcdserverpbKVLeaseGrantHandler;

void AsyncCompleteGrpcetcdserverpbKVLeaseGrant(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVLeaseGrantGrpcClientCall> call(static_cast<AsyncetcdserverpbKVLeaseGrantGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVLeaseGrantHandler){
			AsyncetcdserverpbKVLeaseGrantHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void SendetcdserverpbKVLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseRevokeRequest& request)
{
    AsyncetcdserverpbKVLeaseRevokeGrpcClientCall* call = new AsyncetcdserverpbKVLeaseRevokeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncLeaseRevoke(&call->context, request,
		&registry.get<etcdserverpbKVLeaseRevokeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseRevokeGrpcClientCall>&)>;
AsyncetcdserverpbKVLeaseRevokeHandlerFunctionType  AsyncetcdserverpbKVLeaseRevokeHandler;

void AsyncCompleteGrpcetcdserverpbKVLeaseRevoke(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVLeaseRevokeGrpcClientCall> call(static_cast<AsyncetcdserverpbKVLeaseRevokeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVLeaseRevokeHandler){
			AsyncetcdserverpbKVLeaseRevokeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void SendetcdserverpbKVLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseKeepAliveRequest& request)
{
    AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall* call = new AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncLeaseKeepAlive(&call->context, request,
		&registry.get<etcdserverpbKVLeaseKeepAliveCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVLeaseKeepAliveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall>&)>;
AsyncetcdserverpbKVLeaseKeepAliveHandlerFunctionType  AsyncetcdserverpbKVLeaseKeepAliveHandler;

void AsyncCompleteGrpcetcdserverpbKVLeaseKeepAlive(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall> call(static_cast<AsyncetcdserverpbKVLeaseKeepAliveGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVLeaseKeepAliveHandler){
			AsyncetcdserverpbKVLeaseKeepAliveHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void SendetcdserverpbKVLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseTimeToLiveRequest& request)
{
    AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall* call = new AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncLeaseTimeToLive(&call->context, request,
		&registry.get<etcdserverpbKVLeaseTimeToLiveCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall>&)>;
AsyncetcdserverpbKVLeaseTimeToLiveHandlerFunctionType  AsyncetcdserverpbKVLeaseTimeToLiveHandler;

void AsyncCompleteGrpcetcdserverpbKVLeaseTimeToLive(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall> call(static_cast<AsyncetcdserverpbKVLeaseTimeToLiveGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVLeaseTimeToLiveHandler){
			AsyncetcdserverpbKVLeaseTimeToLiveHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void SendetcdserverpbKVLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::LeaseLeasesRequest& request)
{
    AsyncetcdserverpbKVLeaseLeasesGrpcClientCall* call = new AsyncetcdserverpbKVLeaseLeasesGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncLeaseLeases(&call->context, request,
		&registry.get<etcdserverpbKVLeaseLeasesCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVLeaseLeasesGrpcClientCall>&)>;
AsyncetcdserverpbKVLeaseLeasesHandlerFunctionType  AsyncetcdserverpbKVLeaseLeasesHandler;

void AsyncCompleteGrpcetcdserverpbKVLeaseLeases(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVLeaseLeasesGrpcClientCall> call(static_cast<AsyncetcdserverpbKVLeaseLeasesGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVLeaseLeasesHandler){
			AsyncetcdserverpbKVLeaseLeasesHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void SendetcdserverpbKVWatch(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::WatchRequest& request)
{
    AsyncetcdserverpbKVWatchGrpcClientCall* call = new AsyncetcdserverpbKVWatchGrpcClientCall;

    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncWatch(&call->context, request,
		&registry.get<etcdserverpbKVWatchCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using AsyncetcdserverpbKVWatchHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVWatchGrpcClientCall>&)>;
AsyncetcdserverpbKVWatchHandlerFunctionType  AsyncetcdserverpbKVWatchHandler;

void AsyncCompleteGrpcetcdserverpbKVWatch(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbKVWatchGrpcClientCall> call(static_cast<AsyncetcdserverpbKVWatchGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbKVWatchHandler){
			AsyncetcdserverpbKVWatchHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void SendetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::RangeRequest& request)
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

void SendetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::PutRequest& request)
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

void SendetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::DeleteRangeRequest& request)
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

void SendetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::TxnRequest& request)
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

void SendetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  etcdserverpb::CompactionRequest& request)
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
	registry.emplace<etcdserverpbKVLeaseGrantCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVLeaseRevokeCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVLeaseKeepAliveCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVLeaseTimeToLiveCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVLeaseLeasesCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVWatchCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVRangeCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVPutCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVDeleteRangeCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVTxnCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbKVCompactCompleteQueue>(nodeEntity);
}

void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbKVLeaseGrantCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVLeaseGrant(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVLeaseRevokeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVLeaseRevoke(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVLeaseKeepAliveCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVLeaseKeepAlive(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVLeaseTimeToLiveCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVLeaseTimeToLive(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVLeaseLeasesCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVLeaseLeases(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVWatchCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVWatch(completeQueueComp.cq);
		}
	}
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

