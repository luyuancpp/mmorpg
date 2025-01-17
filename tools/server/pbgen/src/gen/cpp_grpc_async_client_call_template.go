package gen

const AsyncClientHeaderTemplate = `#pragma once

#include "proto/common/{{.ProtoFileBaseName}}.grpc.pb.h"
#include "proto/common/{{.ProtoFileBaseName}}.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

{{- range .GrpcServices }}
class Async{{.ServiceName}}{{.Method}}GrpcClientCall
{
public:
    ClientContext context;
    Status status;

    {{.Response}} reply;
    std::unique_ptr<ClientAsyncResponseReader< {{.Response}}>> response_reader;
};

class {{.Request}};
void {{.ServiceName}}{{.Method}}(const {{.Request}}& request);

{{- end }}`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"

#include "grpc/generator/{{.GeneratorFileName}}.h"

using Grpc{{.ServiceName}}StubPtr = std::unique_ptr<{{.ServiceName}}::Stub>;
Grpc{{.ServiceName}}StubPtr g{{.ServiceName}}Stub;


{{- range .GrpcServices }}

std::unique_ptr<grpc::CompletionQueue> g{{.ServiceName}}{{.Method}}Cq;

void {{.ServiceName}}{{.Method}}(const {{.Request}}& request)
{
    Async{{.ServiceName}}{{.Method}}GrpcClientCall* call = new Async{{.ServiceName}}{{.Method}}GrpcClientCall;

    call->response_reader =
        g{{.ServiceName}}Stub->PrepareAsync{{.Method}}(&call->context, request, g{{.ServiceName}}{{.Method}}Cq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpc{{.ServiceName}}{{.Method}}(grpc::CompletionQueue& cq)
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

    std::unique_ptr<Async{{.ServiceName}}{{.Method}}GrpcClientCall> call(static_cast<Async{{.ServiceName}}{{.Method}}GrpcClientCall*>(got_tag));
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

{{- end }}`
