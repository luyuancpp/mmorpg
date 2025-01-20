#include "muduo/base/Logging.h"

#include "grpc/generator/deploy_service_grpc.h"
#include "thread_local/storage.h"


entt::entity GlobalGrpcNodeEntity();
struct DeployServiceGetNodeInfoCompleteQueue{
	grpc::CompletionQueue cq;
};
struct DeployServiceGetIDCompleteQueue{
	grpc::CompletionQueue cq;
};
struct DeployServiceReleaseIDCompleteQueue{
	grpc::CompletionQueue cq;
};
struct DeployServiceRenewLeaseCompleteQueue{
	grpc::CompletionQueue cq;
};

void DeployServiceGetNodeInfo(entt::registry& registry, entt::entity nodeEntity, const NodeInfoRequest& request)
{
    AsyncDeployServiceGetNodeInfoGrpcClientCall* call = new AsyncDeployServiceGetNodeInfoGrpcClientCall;

    call->response_reader =
        registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncGetNodeInfo(&call->context, request,
		&registry.get<DeployServiceGetNodeInfoCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)> AsyncDeployServiceGetNodeInfoHandler;

void AsyncCompleteGrpcDeployServiceGetNodeInfo(grpc::CompletionQueue& cq)
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

void DeployServiceGetID(entt::registry& registry, entt::entity nodeEntity, const GetIDRequest& request)
{
    AsyncDeployServiceGetIDGrpcClientCall* call = new AsyncDeployServiceGetIDGrpcClientCall;

    call->response_reader =
        registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncGetID(&call->context, request,
		&registry.get<DeployServiceGetIDCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall>&)> AsyncDeployServiceGetIDHandler;

void AsyncCompleteGrpcDeployServiceGetID(grpc::CompletionQueue& cq)
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

void DeployServiceReleaseID(entt::registry& registry, entt::entity nodeEntity, const ReleaseIDRequest& request)
{
    AsyncDeployServiceReleaseIDGrpcClientCall* call = new AsyncDeployServiceReleaseIDGrpcClientCall;

    call->response_reader =
        registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncReleaseID(&call->context, request,
		&registry.get<DeployServiceReleaseIDCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall>&)> AsyncDeployServiceReleaseIDHandler;

void AsyncCompleteGrpcDeployServiceReleaseID(grpc::CompletionQueue& cq)
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

void DeployServiceRenewLease(entt::registry& registry, entt::entity nodeEntity, const RenewLeaseIDRequest& request)
{
    AsyncDeployServiceRenewLeaseGrpcClientCall* call = new AsyncDeployServiceRenewLeaseGrpcClientCall;

    call->response_reader =
        registry.get<GrpcDeployServiceStubPtr>(nodeEntity)->PrepareAsyncRenewLease(&call->context, request,
		&registry.get<DeployServiceRenewLeaseCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall>&)> AsyncDeployServiceRenewLeaseHandler;

void AsyncCompleteGrpcDeployServiceRenewLease(grpc::CompletionQueue& cq)
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

void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<DeployServiceGetNodeInfoCompleteQueue>(nodeEntity);
	registry.emplace<DeployServiceGetIDCompleteQueue>(nodeEntity);
	registry.emplace<DeployServiceReleaseIDCompleteQueue>(nodeEntity);
	registry.emplace<DeployServiceRenewLeaseCompleteQueue>(nodeEntity);
}

void HandleDeployServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<DeployServiceGetNodeInfoCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcDeployServiceGetNodeInfo(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<DeployServiceGetIDCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcDeployServiceGetID(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<DeployServiceReleaseIDCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcDeployServiceReleaseID(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<DeployServiceRenewLeaseCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcDeployServiceRenewLease(completeQueueComp.cq);
		}
	}
}

