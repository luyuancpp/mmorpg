#include "deploy_grpc_requst.h"

#include "src/grpc/async_client_call.h"
#include "src/grpc/deployclient.h"

std::unique_ptr<DeployService::Stub> g_deploy_stub;

void SendGetNodeInfo(NodeInfoRequest& request)
{
    AsyncClientCall* call = new AsyncClientCall;

    call->response_reader =
        g_deploy_stub->PrepareAsyncGetNodeInfo(&call->context, request, &g_deploy_client->cq());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}
