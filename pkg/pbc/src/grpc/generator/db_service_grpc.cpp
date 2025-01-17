#include "muduo/base/Logging.h"

#include "grpc/generator/db_service_grpc.h"

using GrpcAccountDBServiceStubPtr = std::unique_ptr<AccountDBService::Stub>;
GrpcAccountDBServiceStubPtr gAccountDBServiceStub;

std::unique_ptr<grpc::CompletionQueue> gAccountDBServiceLoad2RedisCq;

void AccountDBServiceLoad2Redis(const LoadAccountRequest& request)
{
    AsyncAccountDBServiceLoad2RedisGrpcClientCall* call = new AsyncAccountDBServiceLoad2RedisGrpcClientCall;

    call->response_reader =
        gAccountDBServiceStub->PrepareAsyncLoad2Redis(&call->context, request, gAccountDBServiceLoad2RedisCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcAccountDBServiceLoad2Redis(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall> call(static_cast<AsyncAccountDBServiceLoad2RedisGrpcClientCall*>(got_tag));
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

std::unique_ptr<grpc::CompletionQueue> gAccountDBServiceSave2RedisCq;

void AccountDBServiceSave2Redis(const SaveAccountRequest& request)
{
    AsyncAccountDBServiceSave2RedisGrpcClientCall* call = new AsyncAccountDBServiceSave2RedisGrpcClientCall;

    call->response_reader =
        gAccountDBServiceStub->PrepareAsyncSave2Redis(&call->context, request, gAccountDBServiceSave2RedisCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcAccountDBServiceSave2Redis(grpc::CompletionQueue& cq)
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

    std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall> call(static_cast<AsyncAccountDBServiceSave2RedisGrpcClientCall*>(got_tag));
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