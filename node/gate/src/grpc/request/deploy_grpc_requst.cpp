#include "deploy_grpc_requst.h"

#include "grpc/client/deploy_async_client_call.h"
#include "grpc/deploy/deployclient.h"

std::unique_ptr<DeployService::Stub> gDeployStub;

void SendGetNodeInfo( const NodeInfoRequest& request)
{
    const auto call(new DeployAsyncClientCall);
    call->response_reader =
        gDeployStub->PrepareAsyncGetNodeInfo(&call->context, request, gDeployCq.get());
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}
