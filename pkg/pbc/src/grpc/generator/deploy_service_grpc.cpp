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

void DeployServiceGetNodeInfo(GrpcDeployServiceStubPtr& stub, const NodeInfoRequest& request)
{
    AsyncDeployServiceGetNodeInfoGrpcClientCall* call = new AsyncDeployServiceGetNodeInfoGrpcClientCall;

    call->response_reader =
        stub->PrepareAsyncGetNodeInfo(&call->context, request,
		&tls.grpc_node_registry.get<DeployServiceGetNodeInfoCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)> AsyncDeployServiceGetNodeInfoHandler;

void AsyncCompleteGrpcDeployServiceGetNodeInfo()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<DeployServiceGetNodeInfoCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void DeployServiceGetID(GrpcDeployServiceStubPtr& stub, const GetIDRequest& request)
{
    AsyncDeployServiceGetIDGrpcClientCall* call = new AsyncDeployServiceGetIDGrpcClientCall;

    call->response_reader =
        stub->PrepareAsyncGetID(&call->context, request,
		&tls.grpc_node_registry.get<DeployServiceGetIDCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall>&)> AsyncDeployServiceGetIDHandler;

void AsyncCompleteGrpcDeployServiceGetID()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<DeployServiceGetIDCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void DeployServiceReleaseID(GrpcDeployServiceStubPtr& stub, const ReleaseIDRequest& request)
{
    AsyncDeployServiceReleaseIDGrpcClientCall* call = new AsyncDeployServiceReleaseIDGrpcClientCall;

    call->response_reader =
        stub->PrepareAsyncReleaseID(&call->context, request,
		&tls.grpc_node_registry.get<DeployServiceReleaseIDCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall>&)> AsyncDeployServiceReleaseIDHandler;

void AsyncCompleteGrpcDeployServiceReleaseID()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<DeployServiceReleaseIDCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void DeployServiceRenewLease(GrpcDeployServiceStubPtr& stub, const RenewLeaseIDRequest& request)
{
    AsyncDeployServiceRenewLeaseGrpcClientCall* call = new AsyncDeployServiceRenewLeaseGrpcClientCall;

    call->response_reader =
        stub->PrepareAsyncRenewLease(&call->context, request,
		&tls.grpc_node_registry.get<DeployServiceRenewLeaseCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall>&)> AsyncDeployServiceRenewLeaseHandler;

void AsyncCompleteGrpcDeployServiceRenewLease()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<DeployServiceRenewLeaseCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void InitDeployServiceCompletedQueue() {
	tls.grpc_node_registry.emplace<DeployServiceGetNodeInfoCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<DeployServiceGetIDCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<DeployServiceReleaseIDCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<DeployServiceRenewLeaseCompleteQueue>(GlobalGrpcNodeEntity());
}

void HandleDeployServiceCompletedQueueMessage() {
    AsyncCompleteGrpcDeployServiceGetNodeInfo();
    AsyncCompleteGrpcDeployServiceGetID();
    AsyncCompleteGrpcDeployServiceReleaseID();
    AsyncCompleteGrpcDeployServiceRenewLease();
}

