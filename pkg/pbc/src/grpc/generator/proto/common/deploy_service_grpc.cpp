#include "muduo/base/Logging.h"

#include "deploy_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

struct DeployServiceGetNodeInfoCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncDeployServiceGetNodeInfoHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)>;
AsyncDeployServiceGetNodeInfoHandlerFunctionType  AsyncDeployServiceGetNodeInfoHandler;

void AsyncCompleteGrpcDeployServiceGetNodeInfo(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall> call(static_cast<AsyncDeployServiceGetNodeInfoGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncDeployServiceGetNodeInfoHandler){
			AsyncDeployServiceGetNodeInfoHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}


void SendDeployServiceGetNodeInfo(entt::registry& registry, entt::entity nodeEntity, const  ::NodeInfoRequest& request)
{

    AsyncDeployServiceGetNodeInfoGrpcClientCall* call = new AsyncDeployServiceGetNodeInfoGrpcClientCall;
    call->response_reader =
	registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncGetNodeInfo(&call->context, request,
	&registry.get<DeployServiceGetNodeInfoCompleteQueue>(nodeEntity).cq);

	call->response_reader->StartCall();

	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct DeployServiceGetIDCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncDeployServiceGetIDHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall>&)>;
AsyncDeployServiceGetIDHandlerFunctionType  AsyncDeployServiceGetIDHandler;

void AsyncCompleteGrpcDeployServiceGetID(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall> call(static_cast<AsyncDeployServiceGetIDGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncDeployServiceGetIDHandler){
			AsyncDeployServiceGetIDHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}


void SendDeployServiceGetID(entt::registry& registry, entt::entity nodeEntity, const  ::GetIDRequest& request)
{

    AsyncDeployServiceGetIDGrpcClientCall* call = new AsyncDeployServiceGetIDGrpcClientCall;
    call->response_reader =
	registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncGetID(&call->context, request,
	&registry.get<DeployServiceGetIDCompleteQueue>(nodeEntity).cq);

	call->response_reader->StartCall();

	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct DeployServiceReleaseIDCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncDeployServiceReleaseIDHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall>&)>;
AsyncDeployServiceReleaseIDHandlerFunctionType  AsyncDeployServiceReleaseIDHandler;

void AsyncCompleteGrpcDeployServiceReleaseID(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall> call(static_cast<AsyncDeployServiceReleaseIDGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncDeployServiceReleaseIDHandler){
			AsyncDeployServiceReleaseIDHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}


void SendDeployServiceReleaseID(entt::registry& registry, entt::entity nodeEntity, const  ::ReleaseIDRequest& request)
{

    AsyncDeployServiceReleaseIDGrpcClientCall* call = new AsyncDeployServiceReleaseIDGrpcClientCall;
    call->response_reader =
	registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncReleaseID(&call->context, request,
	&registry.get<DeployServiceReleaseIDCompleteQueue>(nodeEntity).cq);

	call->response_reader->StartCall();

	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct DeployServiceRenewLeaseCompleteQueue{
	grpc::CompletionQueue cq;
};


using AsyncDeployServiceRenewLeaseHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall>&)>;
AsyncDeployServiceRenewLeaseHandlerFunctionType  AsyncDeployServiceRenewLeaseHandler;

void AsyncCompleteGrpcDeployServiceRenewLease(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall> call(static_cast<AsyncDeployServiceRenewLeaseGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncDeployServiceRenewLeaseHandler){
			AsyncDeployServiceRenewLeaseHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}


void SendDeployServiceRenewLease(entt::registry& registry, entt::entity nodeEntity, const  ::RenewLeaseIDRequest& request)
{

    AsyncDeployServiceRenewLeaseGrpcClientCall* call = new AsyncDeployServiceRenewLeaseGrpcClientCall;
    call->response_reader =
	registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncRenewLease(&call->context, request,
	&registry.get<DeployServiceRenewLeaseCompleteQueue>(nodeEntity).cq);

	call->response_reader->StartCall();

	call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<DeployServiceGetNodeInfoCompleteQueue>(nodeEntity);

	registry.emplace<DeployServiceGetIDCompleteQueue>(nodeEntity);

	registry.emplace<DeployServiceReleaseIDCompleteQueue>(nodeEntity);

	registry.emplace<DeployServiceRenewLeaseCompleteQueue>(nodeEntity);

}
void HandleDeployServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<DeployServiceGetNodeInfoCompleteQueue>();
		for (auto&& [e, completeQueueComp] : view.each()) 	{
			AsyncCompleteGrpcDeployServiceGetNodeInfo(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<DeployServiceGetIDCompleteQueue>();
		for (auto&& [e, completeQueueComp] : view.each()) 	{
			AsyncCompleteGrpcDeployServiceGetID(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<DeployServiceReleaseIDCompleteQueue>();
		for (auto&& [e, completeQueueComp] : view.each()) 	{
			AsyncCompleteGrpcDeployServiceReleaseID(registry, e, completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<DeployServiceRenewLeaseCompleteQueue>();
		for (auto&& [e, completeQueueComp] : view.each()) 	{
			AsyncCompleteGrpcDeployServiceRenewLease(registry, e, completeQueueComp.cq);
		}
	}
}
