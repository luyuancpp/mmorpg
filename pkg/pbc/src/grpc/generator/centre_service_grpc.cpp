#include "muduo/base/Logging.h"

#include "grpc/generator/centre_service_grpc.h"

using GrpcCentreServiceStubPtr = std::unique_ptr<CentreService::Stub>;
GrpcCentreServiceStubPtr gCentreServiceStub;

std::unique_ptr<grpc::CompletionQueue> gCentreServiceRegisterGameNodeCq;

void CentreServiceRegisterGameNode(const RegisterGameNodeRequest& request)
{
    AsyncCentreServiceRegisterGameNodeGrpcClientCall* call = new AsyncCentreServiceRegisterGameNodeGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRegisterGameNode(&call->context, request, gCentreServiceRegisterGameNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceRegisterGameNode(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRegisterGameNodeGrpcClientCall> call(static_cast<AsyncCentreServiceRegisterGameNodeGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceRegisterGateNodeCq;

void CentreServiceRegisterGateNode(const RegisterGateNodeRequest& request)
{
    AsyncCentreServiceRegisterGateNodeGrpcClientCall* call = new AsyncCentreServiceRegisterGateNodeGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRegisterGateNode(&call->context, request, gCentreServiceRegisterGateNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceRegisterGateNode(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRegisterGateNodeGrpcClientCall> call(static_cast<AsyncCentreServiceRegisterGateNodeGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceGatePlayerServiceCq;

void CentreServiceGatePlayerService(const GateClientMessageRequest& request)
{
    AsyncCentreServiceGatePlayerServiceGrpcClientCall* call = new AsyncCentreServiceGatePlayerServiceGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncGatePlayerService(&call->context, request, gCentreServiceGatePlayerServiceCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceGatePlayerService(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall> call(static_cast<AsyncCentreServiceGatePlayerServiceGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceGateSessionDisconnectCq;

void CentreServiceGateSessionDisconnect(const GateSessionDisconnectRequest& request)
{
    AsyncCentreServiceGateSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceGateSessionDisconnectGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncGateSessionDisconnect(&call->context, request, gCentreServiceGateSessionDisconnectCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceGateSessionDisconnect(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall> call(static_cast<AsyncCentreServiceGateSessionDisconnectGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceLoginNodeAccountLoginCq;

void CentreServiceLoginNodeAccountLogin(const LoginRequest& request)
{
    AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall* call = new AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeAccountLogin(&call->context, request, gCentreServiceLoginNodeAccountLoginCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceLoginNodeAccountLogin(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceLoginNodeEnterGameCq;

void CentreServiceLoginNodeEnterGame(const CentrePlayerGameNodeEntryRequest& request)
{
    AsyncCentreServiceLoginNodeEnterGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeEnterGameGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeEnterGame(&call->context, request, gCentreServiceLoginNodeEnterGameCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceLoginNodeEnterGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceLoginNodeLeaveGameCq;

void CentreServiceLoginNodeLeaveGame(const LoginNodeLeaveGameRequest& request)
{
    AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall* call = new AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeLeaveGame(&call->context, request, gCentreServiceLoginNodeLeaveGameCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceLoginNodeLeaveGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceLoginNodeSessionDisconnectCq;

void CentreServiceLoginNodeSessionDisconnect(const GateSessionDisconnectRequest& request)
{
    AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall* call = new AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncLoginNodeSessionDisconnect(&call->context, request, gCentreServiceLoginNodeSessionDisconnectCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceLoginNodeSessionDisconnect(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall> call(static_cast<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServicePlayerServiceCq;

void CentreServicePlayerService(const NodeRouteMessageRequest& request)
{
    AsyncCentreServicePlayerServiceGrpcClientCall* call = new AsyncCentreServicePlayerServiceGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncPlayerService(&call->context, request, gCentreServicePlayerServiceCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServicePlayerService(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall> call(static_cast<AsyncCentreServicePlayerServiceGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceEnterGsSucceedCq;

void CentreServiceEnterGsSucceed(const EnterGameNodeSuccessRequest& request)
{
    AsyncCentreServiceEnterGsSucceedGrpcClientCall* call = new AsyncCentreServiceEnterGsSucceedGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncEnterGsSucceed(&call->context, request, gCentreServiceEnterGsSucceedCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceEnterGsSucceed(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall> call(static_cast<AsyncCentreServiceEnterGsSucceedGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceRouteNodeStringMsgCq;

void CentreServiceRouteNodeStringMsg(const RouteMessageRequest& request)
{
    AsyncCentreServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncCentreServiceRouteNodeStringMsgGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRouteNodeStringMsg(&call->context, request, gCentreServiceRouteNodeStringMsgCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceRouteNodeStringMsg(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall> call(static_cast<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceRoutePlayerStringMsgCq;

void CentreServiceRoutePlayerStringMsg(const RoutePlayerMessageRequest& request)
{
    AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncRoutePlayerStringMsg(&call->context, request, gCentreServiceRoutePlayerStringMsgCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceRoutePlayerStringMsg(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall> call(static_cast<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gCentreServiceUnRegisterGameNodeCq;

void CentreServiceUnRegisterGameNode(const UnregisterGameNodeRequest& request)
{
    AsyncCentreServiceUnRegisterGameNodeGrpcClientCall* call = new AsyncCentreServiceUnRegisterGameNodeGrpcClientCall;

    call->response_reader =
        gCentreServiceStub->PrepareAsyncUnRegisterGameNode(&call->context, request, gCentreServiceUnRegisterGameNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcCentreServiceUnRegisterGameNode(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall> call(static_cast<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall*>(got_tag));
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