#include "login_grpc_request.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/completion_queue.h>

#include "thread_local/thread_local_storage_gate.h"
#include "grpc/client/login_async_client_call.h"

using GrpcLoginStupPtr = std::unique_ptr<LoginService::Stub>;

void SendLoginC2LRequest(entt::entity login_node, LoginC2LRequest& request)
{
    if (!tls_gate.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = tls_gate.login_node_registry.get<GrpcLoginStupPtr>(login_node);
    LoginC2LAsyncClientCall* call = new LoginC2LAsyncClientCall;
    call->response_reader = 
        stub->PrepareAsyncLogin(&call->context, request, 
            &tls_gate.login_node_registry.get<LoginC2LCompletionQueue>(login_node).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

void SendCreatePlayerC2LRequest(entt::entity login_node, CreatePlayerC2LRequest& request)
{
    if (!tls_gate.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = tls_gate.login_node_registry.get<GrpcLoginStupPtr>(login_node);
    CreatePlayerC2LAsyncClientCall* call = new CreatePlayerC2LAsyncClientCall;
    call->response_reader =
        stub->PrepareAsyncCreatePlayer(&call->context, request,
            &tls_gate.login_node_registry.get<CreatePlayerC2LCompletionQueue>(login_node).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

void SendEnterGameC2LRequest(entt::entity login_node, EnterGameC2LRequest& request)
{
    if (!tls_gate.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = tls_gate.login_node_registry.get<GrpcLoginStupPtr>(login_node);
    EnterGameC2LAsyncClientCall* call = new EnterGameC2LAsyncClientCall;
    call->response_reader =
        stub->PrepareAsyncEnterGame(&call->context, request,
            &tls_gate.login_node_registry.get<EnterGameC2LCompletionQueue>(login_node).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

