#include "muduo/base/Logging.h"

#include "grpc/generator/gate_service_grpc.h"

using GrpcGateServiceStubPtr = std::unique_ptr<GateService::Stub>;
GrpcGateServiceStubPtr gGateServiceStub;

std::unique_ptr<grpc::CompletionQueue> gGateServiceRegisterGameCq;

void GateServiceRegisterGame(const RegisterGameNodeRequest& request)
{
    AsyncGateServiceRegisterGameGrpcClientCall* call = new AsyncGateServiceRegisterGameGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncRegisterGame(&call->context, request, gGateServiceRegisterGameCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceRegisterGame(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall> call(static_cast<AsyncGateServiceRegisterGameGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServiceUnRegisterGameCq;

void GateServiceUnRegisterGame(const UnregisterGameNodeRequest& request)
{
    AsyncGateServiceUnRegisterGameGrpcClientCall* call = new AsyncGateServiceUnRegisterGameGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncUnRegisterGame(&call->context, request, gGateServiceUnRegisterGameCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceUnRegisterGame(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall> call(static_cast<AsyncGateServiceUnRegisterGameGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServicePlayerEnterGameNodeCq;

void GateServicePlayerEnterGameNode(const RegisterGameNodeSessionRequest& request)
{
    AsyncGateServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGateServicePlayerEnterGameNodeGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncPlayerEnterGameNode(&call->context, request, gGateServicePlayerEnterGameNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServicePlayerEnterGameNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall> call(static_cast<AsyncGateServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServiceSendMessageToPlayerCq;

void GateServiceSendMessageToPlayer(const NodeRouteMessageRequest& request)
{
    AsyncGateServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGateServiceSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncSendMessageToPlayer(&call->context, request, gGateServiceSendMessageToPlayerCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceSendMessageToPlayer(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall> call(static_cast<AsyncGateServiceSendMessageToPlayerGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServiceKickSessionByCentreCq;

void GateServiceKickSessionByCentre(const KickSessionRequest& request)
{
    AsyncGateServiceKickSessionByCentreGrpcClientCall* call = new AsyncGateServiceKickSessionByCentreGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncKickSessionByCentre(&call->context, request, gGateServiceKickSessionByCentreCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceKickSessionByCentre(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall> call(static_cast<AsyncGateServiceKickSessionByCentreGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServiceRouteNodeMessageCq;

void GateServiceRouteNodeMessage(const RouteMessageRequest& request)
{
    AsyncGateServiceRouteNodeMessageGrpcClientCall* call = new AsyncGateServiceRouteNodeMessageGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncRouteNodeMessage(&call->context, request, gGateServiceRouteNodeMessageCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceRouteNodeMessage(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall> call(static_cast<AsyncGateServiceRouteNodeMessageGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServiceRoutePlayerMessageCq;

void GateServiceRoutePlayerMessage(const RoutePlayerMessageRequest& request)
{
    AsyncGateServiceRoutePlayerMessageGrpcClientCall* call = new AsyncGateServiceRoutePlayerMessageGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncRoutePlayerMessage(&call->context, request, gGateServiceRoutePlayerMessageCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceRoutePlayerMessage(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall> call(static_cast<AsyncGateServiceRoutePlayerMessageGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGateServiceBroadcastToPlayersCq;

void GateServiceBroadcastToPlayers(const BroadcastToPlayersRequest& request)
{
    AsyncGateServiceBroadcastToPlayersGrpcClientCall* call = new AsyncGateServiceBroadcastToPlayersGrpcClientCall;

    call->response_reader =
        gGateServiceStub->PrepareAsyncBroadcastToPlayers(&call->context, request, gGateServiceBroadcastToPlayersCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGateServiceBroadcastToPlayers(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall> call(static_cast<AsyncGateServiceBroadcastToPlayersGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}