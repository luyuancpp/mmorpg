#include "muduo/base/Logging.h"

#include "grpc/generator/deploy_service_grpc.h"

using GrpcDeployServiceStubPtr = std::unique_ptr<DeployService::Stub>;
GrpcDeployServiceStubPtr gDeployServiceStub;

std::unique_ptr<grpc::CompletionQueue> gDeployServiceGetNodeInfoCq;

void DeployServiceGetNodeInfo(const NodeInfoRequest& request)
{
    AsyncDeployServiceGetNodeInfoGrpcClientCall* call = new AsyncDeployServiceGetNodeInfoGrpcClientCall;

    call->response_reader =
        gDeployServiceStub->PrepareAsyncGetNodeInfo(&call->context, request, gDeployServiceGetNodeInfoCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcDeployServiceGetNodeInfo(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall> call(static_cast<AsyncDeployServiceGetNodeInfoGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gDeployServiceGetIDCq;

void DeployServiceGetID(const GetIDRequest& request)
{
    AsyncDeployServiceGetIDGrpcClientCall* call = new AsyncDeployServiceGetIDGrpcClientCall;

    call->response_reader =
        gDeployServiceStub->PrepareAsyncGetID(&call->context, request, gDeployServiceGetIDCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcDeployServiceGetID(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall> call(static_cast<AsyncDeployServiceGetIDGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gDeployServiceReleaseIDCq;

void DeployServiceReleaseID(const ReleaseIDRequest& request)
{
    AsyncDeployServiceReleaseIDGrpcClientCall* call = new AsyncDeployServiceReleaseIDGrpcClientCall;

    call->response_reader =
        gDeployServiceStub->PrepareAsyncReleaseID(&call->context, request, gDeployServiceReleaseIDCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcDeployServiceReleaseID(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall> call(static_cast<AsyncDeployServiceReleaseIDGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gDeployServiceRenewLeaseCq;

void DeployServiceRenewLease(const RenewLeaseIDRequest& request)
{
    AsyncDeployServiceRenewLeaseGrpcClientCall* call = new AsyncDeployServiceRenewLeaseGrpcClientCall;

    call->response_reader =
        gDeployServiceStub->PrepareAsyncRenewLease(&call->context, request, gDeployServiceRenewLeaseCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcDeployServiceRenewLease(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall> call(static_cast<AsyncDeployServiceRenewLeaseGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}