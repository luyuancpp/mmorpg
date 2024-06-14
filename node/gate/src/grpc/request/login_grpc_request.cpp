#include "login_grpc_request.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/completion_queue.h>

#include "thread_local/gate_thread_local_storage.h"
#include "grpc/client/login_async_client_call.h"

using grpc::CompletionQueue;

CompletionQueue LoginC2LCQ;

void SendLoginC2LRequest(LoginC2LRequest& request, NodeId login_node_id)
{
    entt::entity login_node = entt::entity{login_node_id};
    if (!gate_tls.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = gate_tls.login_node_registry.get<std::unique_ptr<LoginService::Stub>>(login_node);
    LoginC2LAsyncClientCall* call = new LoginC2LAsyncClientCall;
    call->response_reader = 
        stub->PrepareAsyncLogin(&call->context, request, &LoginC2LCQ);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

CompletionQueue CreatePlayerC2LRequestCQ;
void SendCreatePlayerC2LRequest(CreatePlayerC2LRequest& request, NodeId login_node_id)
{
    entt::entity login_node = entt::entity{ login_node_id };
    if (!gate_tls.login_node_registry.valid(login_node))
    {
        return;
    }
    auto& stub = gate_tls.login_node_registry.get<std::unique_ptr<LoginService::Stub>>(login_node);
    CreatePlayerC2LAsyncClientCall* call = new CreatePlayerC2LAsyncClientCall;
    call->response_reader =
        stub->PrepareAsyncCreatePlayer(&call->context, request, &LoginC2LCQ);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}