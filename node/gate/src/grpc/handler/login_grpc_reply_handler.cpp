
#include "muduo/base/Logging.h"

#include "util/defer.h"

#include "thread_local/thread_local_storage_gate.h"
#include "grpc/client/login_async_client_call.h"
#include "gate_node.h"
#include "network/gate_session.h"
#include "grpc/request/login_grpc_request.h"

using GrpcLoginStupPtr = std::unique_ptr<LoginService::Stub>;

void AsyncCompleteGrpcLoginC2L(CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<LoginC2LAsyncClientCall> call(static_cast<LoginC2LAsyncClientCall*>(got_tag));

    CHECK(ok);

    if (call->status.ok())
    {
        entt::entity session_id{ call->reply.session_info().session_id() };
        if (!tls.session_registry.valid(session_id))
        {
            return;
        }
        auto session = tls.session_registry.try_get<Session>(session_id);
        if (nullptr == session)
        {
            return;
        }
        g_gate_node->Send2Client(session->conn_, call->reply.client_msg_body());
    }
    else
    {
        LOG_INFO << "RPC failed";
    }
}


void AsyncCompleteCreatePlayerC2L(CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<CreatePlayerC2LAsyncClientCall> call(static_cast<CreatePlayerC2LAsyncClientCall*>(got_tag));

    CHECK(ok);

    if (call->status.ok())
    {
        entt::entity session_id{ call->reply.session_info().session_id() };
        if (!tls.session_registry.valid(session_id))
        {
            return;
        }
        auto session = tls.session_registry.try_get<Session>(session_id);
        if (nullptr == session)
        {
            return;
        }
        g_gate_node->Send2Client(session->conn_, call->reply.client_msg_body());
    }
    else
    {
        LOG_INFO << "RPC failed";
    }   
}

void AsyncCompleteEnterGameC2L(CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<EnterGameC2LAsyncClientCall> call(static_cast<EnterGameC2LAsyncClientCall*>(got_tag));

    CHECK(ok);

    if (call->status.ok())
    {
        entt::entity session_id{ call->reply.session_info().session_id() };
        if (!tls.session_registry.valid(session_id))
        {
            return;
        }
        auto session = tls.session_registry.try_get<Session>(session_id);
        if (nullptr == session)
        {
            return;
        }
        g_gate_node->Send2Client(session->conn_, call->reply.client_msg_body());
    }
    else
    {
        LOG_INFO << "RPC failed";
    }
}

void InitLoginNodeComponent()
{
    for (auto&& e : tls_gate.login_node_registry.view<GrpcLoginStupPtr>())
    {
        tls_gate.login_node_registry.emplace<LoginC2LCompletionQueue>(e);
        tls_gate.login_node_registry.emplace<CreatePlayerC2LCompletionQueue>(e);
        tls_gate.login_node_registry.emplace<EnterGameC2LCompletionQueue>(e);
    }
}

void AsyncCompleteRpcLoginService()
{
    for (auto&& e : tls_gate.login_node_registry.view<GrpcLoginStupPtr>())
    {
        AsyncCompleteGrpcLoginC2L(
            tls_gate.login_node_registry.get<LoginC2LCompletionQueue>(e).cq);
        AsyncCompleteCreatePlayerC2L(
            tls_gate.login_node_registry.get<CreatePlayerC2LCompletionQueue>(e).cq);
        AsyncCompleteEnterGameC2L(
            tls_gate.login_node_registry.get<EnterGameC2LCompletionQueue>(e).cq);
    }
}