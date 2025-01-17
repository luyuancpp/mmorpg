#include "deploy_grpc_requst.h"

#include "grpc/client/deploy_async_client_call.h"
#include "grpc/deploy/deploy_client.h"

std::unique_ptr<DeployService::Stub> gDeployStub;

void SendGetNodeInfo(const NodeInfoRequest& request)
{
    DeployAsyncGetNodeInfoClientCall* call = new DeployAsyncGetNodeInfoClientCall;

    call->response_reader =
        gDeployStub->PrepareAsyncGetNodeInfo(&call->context, request, gDeployCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void GetID(const GetIDRequest& request) {
    DeployAsyncGetIDClientCall* call = new DeployAsyncGetIDClientCall;

    call->response_reader =
        gDeployStub->PrepareAsyncGetID(&call->context, request, gDeployCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

void ReleaseID(const ReleaseIDRequest& request) {
    DeployAsyncReleaseIDClientCall* call = new DeployAsyncReleaseIDClientCall;

    call->response_reader =
        gDeployStub->PrepareAsyncReleaseID(&call->context, request, gDeployCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

void RenewLease(const RenewLeaseIDRequest& request) {
    DeployAsyncRenewLeaseIDClientCall* call = new DeployAsyncRenewLeaseIDClientCall;

    call->response_reader =
        gDeployStub->PrepareAsyncRenewLease(&call->context, request, gDeployCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}