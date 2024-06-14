#include "deploy_grpc_requst.h"

#include "grpc/client/deploy_async_client_call.h"
#include "grpc/deploy/deployclient.h"

std::unique_ptr<DeployService::Stub> g_deploy_stub;

void SendGetNodeInfo(NodeInfoRequest& request)
{
    DeployAsyncClientCall* call = new DeployAsyncClientCall;
    call->response_reader =
        g_deploy_stub->PrepareAsyncGetNodeInfo(&call->context, request, g_deploy_cq.get());
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}
