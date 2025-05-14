#include "muduo/base/Logging.h"

#include "etcd_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

struct etcdserverpbKVRangeCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbKVRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>&)>;
AsyncetcdserverpbKVRangeHandlerFunctionType  AsyncetcdserverpbKVRangeHandler;

void AsyncCompleteGrpcetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbKVRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::RangeRequest& request)
{

    AsyncetcdserverpbKVRangeGrpcClientCall* call = new AsyncetcdserverpbKVRangeGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncRange(&call->context, request,
		&registry.get<etcdserverpbKVRangeCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbKVPutCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbKVPutHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>&)>;
AsyncetcdserverpbKVPutHandlerFunctionType  AsyncetcdserverpbKVPutHandler;

void AsyncCompleteGrpcetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbKVPut(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::PutRequest& request)
{

    AsyncetcdserverpbKVPutGrpcClientCall* call = new AsyncetcdserverpbKVPutGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncPut(&call->context, request,
		&registry.get<etcdserverpbKVPutCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbKVDeleteRangeCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbKVDeleteRangeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>&)>;
AsyncetcdserverpbKVDeleteRangeHandlerFunctionType  AsyncetcdserverpbKVDeleteRangeHandler;

void AsyncCompleteGrpcetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::DeleteRangeRequest& request)
{

    AsyncetcdserverpbKVDeleteRangeGrpcClientCall* call = new AsyncetcdserverpbKVDeleteRangeGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncDeleteRange(&call->context, request,
		&registry.get<etcdserverpbKVDeleteRangeCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbKVTxnCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbKVTxnHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>&)>;
AsyncetcdserverpbKVTxnHandlerFunctionType  AsyncetcdserverpbKVTxnHandler;

void AsyncCompleteGrpcetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbKVTxn(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::TxnRequest& request)
{

    AsyncetcdserverpbKVTxnGrpcClientCall* call = new AsyncetcdserverpbKVTxnGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncTxn(&call->context, request,
		&registry.get<etcdserverpbKVTxnCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbKVCompactCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbKVCompactHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbKVCompactGrpcClientCall>&)>;
AsyncetcdserverpbKVCompactHandlerFunctionType  AsyncetcdserverpbKVCompactHandler;

void AsyncCompleteGrpcetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbKVCompact(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::CompactionRequest& request)
{

    AsyncetcdserverpbKVCompactGrpcClientCall* call = new AsyncetcdserverpbKVCompactGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbKVStubPtr>(nodeEntity)->PrepareAsyncCompact(&call->context, request,
		&registry.get<etcdserverpbKVCompactCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbWatchWatchCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbWatchWatchHandlerFunctionType = std::function<void(const ::etcdserverpb::WatchResponse&)>;
AsyncetcdserverpbWatchWatchHandlerFunctionType  AsyncetcdserverpbWatchWatchHandler;

void TryWriteNextNextetcdserverpbWatchWatch(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
{
	auto&  writeInProgress = registry.get<WatchRequestWriteInProgress>(nodeEntity);
	auto&  pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;

	if (writeInProgress.isInProgress){
		return;
	}
	if (pendingWritesBuffer.empty()){
		return;
	}

	auto& client = registry.get<AsyncetcdserverpbWatchWatchGrpcClient>(nodeEntity);
	auto& request = pendingWritesBuffer.front();
	writeInProgress.isInProgress = true;
	client.stream->Write(request,  (void*)(GrpcOperation::WRITE));		
}

void AsyncCompleteGrpcetcdserverpbWatchWatch(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
{
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

	auto& client = registry.get<AsyncetcdserverpbWatchWatchGrpcClient>(nodeEntity);
	auto& writeInProgress = registry.get<WatchRequestWriteInProgress>(nodeEntity);

	switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))){
	case GrpcOperation::WRITE:{
				auto& pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
				if (!pendingWritesBuffer.empty()) {
					pendingWritesBuffer.pop_front();
				}

				writeInProgress.isInProgress = false;

				// 写完之后尝试继续写（而不是触发 Read）
				TryWriteNextNextetcdserverpbWatchWatch(registry, nodeEntity, cq);
				break;
			}
		case GrpcOperation::WRITES_DONE:
			client.stream->Finish(&client.status,  (void*)(GrpcOperation::FINISH));
			break;
		case GrpcOperation::FINISH:
			cq.Shutdown();
			break;
		case GrpcOperation::READ:{
				auto& response = registry.get<::etcdserverpb::WatchResponse>(nodeEntity);

				if(AsyncetcdserverpbWatchWatchHandler){
					AsyncetcdserverpbWatchWatchHandler(response);
				}

				client.stream->Read(&response, (void*)GrpcOperation::READ);  // 🔁 持续读
				TryWriteNextNextetcdserverpbWatchWatch(registry, nodeEntity, cq);  // 📝 写
				break;
			}
		case GrpcOperation::INIT: {
			// 初始化成功后，触发第一次 Read
			auto& response = registry.get<::etcdserverpb::WatchResponse>(nodeEntity);
			client.stream->Read(&response, (void*)GrpcOperation::READ);

			// 初始化完成后，也可以选择尝试第一次 Write
			TryWriteNextNextetcdserverpbWatchWatch(registry, nodeEntity, cq);
			break;
		}

		default:
			break;
	}
}



void SendetcdserverpbWatchWatch(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::WatchRequest& request)
{

	auto& cq = registry.get<etcdserverpbWatchWatchCompleteQueue>(nodeEntity).cq;
	auto&  pendingWritesBuffer = registry.get<WatchRequestBuffer>(nodeEntity).pendingWritesBuffer;
	pendingWritesBuffer.push_back(request);
	TryWriteNextNextetcdserverpbWatchWatch(registry, nodeEntity, cq);

}

struct etcdserverpbLeaseLeaseGrantCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseGrantHandlerFunctionType  AsyncetcdserverpbLeaseLeaseGrantHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseGrantRequest& request)
{

    AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseGrant(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseGrantCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbLeaseLeaseRevokeCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseRevokeHandlerFunctionType  AsyncetcdserverpbLeaseLeaseRevokeHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseRevokeRequest& request)
{

    AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseRevokeGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseRevoke(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseRevokeCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbLeaseLeaseKeepAliveCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType = std::function<void(const ::etcdserverpb::LeaseKeepAliveResponse&)>;
AsyncetcdserverpbLeaseLeaseKeepAliveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseKeepAliveHandler;

void TryWriteNextNextetcdserverpbLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
{
	auto&  writeInProgress = registry.get<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);
	auto&  pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;

	if (writeInProgress.isInProgress){
		return;
	}
	if (pendingWritesBuffer.empty()){
		return;
	}

	auto& client = registry.get<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
	auto& request = pendingWritesBuffer.front();
	writeInProgress.isInProgress = true;
	client.stream->Write(request,  (void*)(GrpcOperation::WRITE));		
}

void AsyncCompleteGrpcetcdserverpbLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
{
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

	auto& client = registry.get<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
	auto& writeInProgress = registry.get<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);

	switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))){
	case GrpcOperation::WRITE:{
			auto& pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
			if (!pendingWritesBuffer.empty()) {
				pendingWritesBuffer.pop_front();
			}

			writeInProgress.isInProgress = false;

			// 写完之后尝试继续写（而不是触发 Read）
			TryWriteNextNextetcdserverpbLeaseLeaseKeepAlive(registry, nodeEntity, cq);  // 📝 写
			break;
		}

		case GrpcOperation::WRITES_DONE:
			client.stream->Finish(&client.status,  (void*)(GrpcOperation::FINISH));
			break;
		case GrpcOperation::FINISH:
			cq.Shutdown();
			break;
		case GrpcOperation::READ:{
				auto& response = registry.get<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);

				if (AsyncetcdserverpbLeaseLeaseKeepAliveHandler) {
					AsyncetcdserverpbLeaseLeaseKeepAliveHandler(response);
				}

				client.stream->Read(&response, (void*)GrpcOperation::READ);  // 🔁 持续读
				TryWriteNextNextetcdserverpbLeaseLeaseKeepAlive(registry, nodeEntity, cq);  // 📝 写
				break;
			}

		case GrpcOperation::INIT: {
			auto& response = registry.get<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);
			client.stream->Read(&response, (void*)GrpcOperation::READ);  // 第一次 read

			// 如果 buffer 里已经有请求，也可以尝试触发 write
			TryWriteNextNextetcdserverpbLeaseLeaseKeepAlive(registry, nodeEntity, cq);
			break;
		}
		default:
			break;
		}

}



void SendetcdserverpbLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseKeepAliveRequest& request)
{

	auto& cq = registry.get<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>(nodeEntity).cq;
	auto&  pendingWritesBuffer = registry.get<LeaseKeepAliveRequestBuffer>(nodeEntity).pendingWritesBuffer;
	pendingWritesBuffer.push_back(request);
	TryWriteNextNextetcdserverpbLeaseLeaseKeepAlive(registry, nodeEntity, cq);

}

struct etcdserverpbLeaseLeaseTimeToLiveCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseTimeToLiveHandlerFunctionType  AsyncetcdserverpbLeaseLeaseTimeToLiveHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseTimeToLiveRequest& request)
{

    AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseTimeToLiveGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseTimeToLive(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseTimeToLiveCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct etcdserverpbLeaseLeaseLeasesCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall>&)>;
AsyncetcdserverpbLeaseLeaseLeasesHandlerFunctionType  AsyncetcdserverpbLeaseLeaseLeasesHandler;

void AsyncCompleteGrpcetcdserverpbLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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


void SendetcdserverpbLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const  ::etcdserverpb::LeaseLeasesRequest& request)
{

    AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall* call = new AsyncetcdserverpbLeaseLeaseLeasesGrpcClientCall;
    call->response_reader =
        registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->PrepareAsyncLeaseLeases(&call->context, request,
		&registry.get<etcdserverpbLeaseLeaseLeasesCompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

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

	{
		auto& client = registry.emplace<AsyncetcdserverpbWatchWatchGrpcClient>(nodeEntity);
		registry.emplace<WatchRequestBuffer>(nodeEntity);
		registry.emplace<WatchRequestWriteInProgress>(nodeEntity);
		registry.emplace<::etcdserverpb::WatchResponse>(nodeEntity);
		registry.emplace<::etcdserverpb::WatchRequest>(nodeEntity);
		client.stream =
			registry.get<GrpcetcdserverpbWatchStubPtr>(nodeEntity)->AsyncWatch(&client.context, 
			&registry.get<etcdserverpbWatchWatchCompleteQueue>(nodeEntity).cq, (void*)(GrpcOperation::INIT));
		
	}

}
void InitetcdserverpbLeaseCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<etcdserverpbLeaseLeaseGrantCompleteQueue>(nodeEntity);

	registry.emplace<etcdserverpbLeaseLeaseRevokeCompleteQueue>(nodeEntity);

	registry.emplace<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>(nodeEntity);

	{
		auto& client = registry.emplace<AsyncetcdserverpbLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
		registry.emplace<LeaseKeepAliveRequestBuffer>(nodeEntity);
		registry.emplace<LeaseKeepAliveRequestWriteInProgress>(nodeEntity);
		registry.emplace<::etcdserverpb::LeaseKeepAliveResponse>(nodeEntity);
		registry.emplace<::etcdserverpb::LeaseKeepAliveRequest>(nodeEntity);
		client.stream =
			registry.get<GrpcetcdserverpbLeaseStubPtr>(nodeEntity)->AsyncLeaseKeepAlive(&client.context, 
			&registry.get<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>(nodeEntity).cq, (void*)(GrpcOperation::INIT));
		
	}

	registry.emplace<etcdserverpbLeaseLeaseTimeToLiveCompleteQueue>(nodeEntity);

	registry.emplace<etcdserverpbLeaseLeaseLeasesCompleteQueue>(nodeEntity);

}
void HandleetcdserverpbKVCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbKVRangeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVRange(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVPutCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVPut(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVDeleteRangeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVDeleteRange(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVTxnCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVTxn(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbKVCompactCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbKVCompact(registry, e, completeQueueComp.cq);
		}
	}
}
void HandleetcdserverpbWatchCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbWatchWatchCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbWatchWatch(registry, e, completeQueueComp.cq);
		}
	}
}
void HandleetcdserverpbLeaseCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseGrantCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseGrant(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseRevokeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseRevoke(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseKeepAliveCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseKeepAlive(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseTimeToLiveCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseTimeToLive(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<etcdserverpbLeaseLeaseLeasesCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcetcdserverpbLeaseLeaseLeases(registry, e, completeQueueComp.cq);
		}
	}
}
