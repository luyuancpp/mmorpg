#include "login_grpc_request.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/completion_queue.h>

#include "thread_local/gate_thread_local_storage.h"
#include "grpc/client/login_async_client_call.h"

using GrpcLoginStupPtr = std::unique_ptr<LoginService::Stub>;

void SendLoginC2LRequest(entt::entity login_node, LoginC2LRequest& request)
{
    if (!gate_tls.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = gate_tls.login_node_registry.get<GrpcLoginStupPtr>(login_node);
    LoginC2LAsyncClientCall* call = new LoginC2LAsyncClientCall;
    call->response_reader = 
        stub->PrepareAsyncLogin(&call->context, request, 
            &gate_tls.login_node_registry.get<LoginC2LCompletionQueue>(login_node).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

void SendCreatePlayerC2LRequest(entt::entity login_node, CreatePlayerC2LRequest& request)
{
    if (!gate_tls.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = gate_tls.login_node_registry.get<GrpcLoginStupPtr>(login_node);
    CreatePlayerC2LAsyncClientCall* call = new CreatePlayerC2LAsyncClientCall;
    call->response_reader =
        stub->PrepareAsyncCreatePlayer(&call->context, request,
            &gate_tls.login_node_registry.get<CreatePlayerC2LCompletionQueue>(login_node).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

