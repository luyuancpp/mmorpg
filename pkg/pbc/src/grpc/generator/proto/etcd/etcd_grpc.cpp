#include "muduo/base/Logging.h"

#include "etcd_grpc.h"
#include "thread_local/storage.h"
struct etcdserverpbKVRangeCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::RangeRequest& request)
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
struct etcdserverpbKVPutCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::PutRequest& request)
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
struct etcdserverpbKVDeleteRangeCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::DeleteRangeRequest& request)
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
struct etcdserverpbKVTxnCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::TxnRequest& request)
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
struct etcdserverpbKVCompactCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::CompactionRequest& request)
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
struct etcdserverpbWatchWatchCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbWatchWatch(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::WatchRequest& request)
{
    AsyncetcdserverpbWatchWatchGrpcClientCall* call = new AsyncetcdserverpbWatchWatchGrpcClientCall;


    call->stream =
        registry.get<GrpcetcdserverpbWatchStubPtr>(nodeEntity)->PrepareAsyncWatch(&call->context, 
		&registry.get<etcdserverpbWatchWatchCompleteQueue>(nodeEntity).cq);

    	call->stream->Write(request, (void*)call);


}

using AsyncetcdserverpbWatchWatchHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbWatchWatchGrpcClientCall>&)>;
AsyncetcdserverpbWatchWatchHandlerFunctionType  AsyncetcdserverpbWatchWatchHandler;

void AsyncCompleteGrpcetcdserverpbWatchWatch(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbWatchWatchGrpcClientCall> call(static_cast<AsyncetcdserverpbWatchWatchGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbWatchWatchHandler){
			AsyncetcdserverpbWatchWatchHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct etcdserverpbLeaseLeaseGrantCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseGrantRequest& request)
{
    AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall;


    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseGrant(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseGrantCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType  AsyncetcdserverpbLeaseLeaseGrantHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseGrant(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall> call(static_cast<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbLeaseLeaseGrantHandler){
			AsyncetcdserverpbLeaseLeaseGrantHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct etcdserverpbLeaseLeaseRevokeCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseRevokeRequest& request)
{
    AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall;


    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseRevoke(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseRevokeCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType  AsyncetcdserverpbLeaseLeaseRevokeHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseRevoke(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall> call(static_cast<AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbLeaseLeaseRevokeHandler){
			AsyncetcdserverpbLeaseLeaseRevokeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct etcdserverpbLeaseLeaseKeepAliveCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseKeepAliveRequest& request)
{
    AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall;


    call->stream =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseKeepAlive(&call->context, 
		&registry.get<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>(nodeEntity).cq);

    	call->stream->Write(request, (void*)call);


}

using AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseKeepAliveHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseKeepAlive(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall> call(static_cast<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbLeaseLeaseKeepAliveHandler){
			AsyncetcdserverpbLeaseLeaseKeepAliveHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct etcdserverpbLeaseLeaseTimeToLiveCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseTimeToLiveRequest& request)
{
    AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall;


    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseTimeToLive(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseTimeToLiveCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseTimeToLiveHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseTimeToLive(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall> call(static_cast<AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbLeaseLeaseTimeToLiveHandler){
			AsyncetcdserverpbLeaseLeaseTimeToLiveHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
struct etcdserverpbLeaseLeaseLeasesCompleteQueue{
	grpc::CompletionQueue cq;
};

void SendetcdserverpbLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseLeasesRequest& request)
{
    AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall;


    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseLeases(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseLeasesCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

using AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType  AsyncetcdserverpbLeaseLeaseLeasesHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseLeases(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall> call(static_cast<AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncetcdserverpbLeaseLeaseLeasesHandler){
			AsyncetcdserverpbLeaseLeaseLeasesHandler(call);
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
void InitetcdserverpbWatchCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<etcdserverpbWatchWatchCompleteQueue>(nodeEntity);
}
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<etcdserverpbLeaseLeaseGrantCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbLeaseLeaseRevokeCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbLeaseLeaseTimeToLiveCompleteQueue>(nodeEntity);
	registry.emplace<etcdserverpbLeaseLeaseLeasesCompleteQueue>(nodeEntity);
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
void HandleetcdserverpbWatchCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbWatchWatchCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbWatchWatch(completeQueueComp.cq);
		}
	}
}
void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseGrantCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseGrant(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseRevokeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseRevoke(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseKeepAlive(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseTimeToLiveCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseTimeToLive(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseLeasesCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseLeases(completeQueueComp.cq);
		}
	}
}
