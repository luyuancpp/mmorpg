#include "deploy_grpc_requst.h"

#include "grpc/async_client_call.h"
#include "grpc/deploy/deployclient.h"

std::unique_ptr<DeployService::Stub> gDeployStub;

void SendGetNodeInfo( const NodeInfoRequest& request)
{
    DeployAsyncClientCall* call = new DeployAsyncClientCall;

    call->response_reader =
        gDeployStub->PrepareAsyncGetNodeInfo(&call->context, request, gDeployCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}
