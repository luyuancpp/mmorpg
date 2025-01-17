#include "muduo/base/Logging.h"

#include "grpc/generator/login_service_grpc.h"

using GrpcLoginServiceStubPtr = std::unique_ptr<LoginService::Stub>;
GrpcLoginServiceStubPtr gLoginServiceStub;

std::unique_ptr<grpc::CompletionQueue> gLoginServiceLoginCq;

void LoginServiceLogin(const LoginC2LRequest& request)
{
    AsyncLoginServiceLoginGrpcClientCall* call = new AsyncLoginServiceLoginGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncLogin(&call->context, request, gLoginServiceLoginCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcLoginServiceLogin(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall> call(static_cast<AsyncLoginServiceLoginGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gLoginServiceCreatePlayerCq;

void LoginServiceCreatePlayer(const CreatePlayerC2LRequest& request)
{
    AsyncLoginServiceCreatePlayerGrpcClientCall* call = new AsyncLoginServiceCreatePlayerGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncCreatePlayer(&call->context, request, gLoginServiceCreatePlayerCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcLoginServiceCreatePlayer(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall> call(static_cast<AsyncLoginServiceCreatePlayerGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gLoginServiceEnterGameCq;

void LoginServiceEnterGame(const EnterGameC2LRequest& request)
{
    AsyncLoginServiceEnterGameGrpcClientCall* call = new AsyncLoginServiceEnterGameGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncEnterGame(&call->context, request, gLoginServiceEnterGameCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcLoginServiceEnterGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall> call(static_cast<AsyncLoginServiceEnterGameGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gLoginServiceLeaveGameCq;

void LoginServiceLeaveGame(const LeaveGameC2LRequest& request)
{
    AsyncLoginServiceLeaveGameGrpcClientCall* call = new AsyncLoginServiceLeaveGameGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncLeaveGame(&call->context, request, gLoginServiceLeaveGameCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcLoginServiceLeaveGame(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall> call(static_cast<AsyncLoginServiceLeaveGameGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gLoginServiceDisconnectCq;

void LoginServiceDisconnect(const LoginNodeDisconnectRequest& request)
{
    AsyncLoginServiceDisconnectGrpcClientCall* call = new AsyncLoginServiceDisconnectGrpcClientCall;

    call->response_reader =
        gLoginServiceStub->PrepareAsyncDisconnect(&call->context, request, gLoginServiceDisconnectCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcLoginServiceDisconnect(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall> call(static_cast<AsyncLoginServiceDisconnectGrpcClientCall*>(got_tag));
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