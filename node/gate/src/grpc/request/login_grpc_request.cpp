#include "login_grpc_request.h"

#include <grpcpp/grpcpp.h>

#include "grpc/client/login_async_client_call.h"
#include "thread_local/storage_gate.h"

using GrpcLoginStubPtr = std::unique_ptr<LoginService::Stub>;

void SendLoginC2LRequest(entt::entity login_node, const LoginC2LRequest& request)
{
    if (!tls_gate.login_node_registry.valid(login_node))
    {
        return;
    }
    const auto& stub      = tls_gate.login_node_registry.get<GrpcLoginStubPtr>(login_node);
    auto        call(new LoginC2LAsyncClientCall);
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
    const auto& stub      = tls_gate.login_node_registry.get<GrpcLoginStubPtr>(login_node);
    const auto  call(new CreatePlayerC2LAsyncClientCall);
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
    const auto& stub      = tls_gate.login_node_registry.get<GrpcLoginStubPtr>(login_node);
    const auto  call(new EnterGameC2LAsyncClientCall);
    call->response_reader =
        stub->PrepareAsyncEnterGame(&call->context, request,
            &tls_gate.login_node_registry.get<EnterGameC2LCompletionQueue>(login_node).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

