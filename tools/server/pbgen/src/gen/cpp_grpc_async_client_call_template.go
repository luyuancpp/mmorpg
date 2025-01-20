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

{{- end }}

void Handle{{.ServiceName}}CompletedQueueMessage(); 
`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"

#include "grpc/generator/{{.GeneratorFileName}}.h"
#include "thread_local/storage.h"

using Grpc{{.ServiceName}}StubPtr = std::unique_ptr<{{.ServiceName}}::Stub>;
Grpc{{.ServiceName}}StubPtr g{{.ServiceName}}Stub;

entt::entity GlobalGrpcNodeEntity();

{{- range .GrpcServices }}
struct {{.ServiceName}}{{.Method}}CompleteQueue{
	grpc::CompletionQueue cq;
};
{{- end }}

{{- range .GrpcServices }}

void {{.ServiceName}}{{.Method}}(const {{.Request}}& request)
{
    Async{{.ServiceName}}{{.Method}}GrpcClientCall* call = new Async{{.ServiceName}}{{.Method}}GrpcClientCall;

    call->response_reader =
        g{{.ServiceName}}Stub->PrepareAsync{{.Method}}(&call->context, request,
		&tls.grpc_node_registry.get<{{.ServiceName}}{{.Method}}CompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<Async{{.ServiceName}}{{.Method}}GrpcClientCall>&)> Async{{.ServiceName}}{{.Method}}Handler;

void AsyncCompleteGrpc{{.ServiceName}}{{.Method}}()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<{{.ServiceName}}{{.Method}}CompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<Async{{.ServiceName}}{{.Method}}GrpcClientCall> call(static_cast<Async{{.ServiceName}}{{.Method}}GrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(Async{{.ServiceName}}{{.Method}}Handler){
			Async{{.ServiceName}}{{.Method}}Handler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
{{- end }}

void Init{{.ServiceName}}CompletedQueue() {
{{- range .GrpcServices }}
	tls.grpc_node_registry.emplace<{{.ServiceName}}{{.Method}}CompleteQueue>(GlobalGrpcNodeEntity());
{{- end }}
}

void Handle{{.ServiceName}}CompletedQueueMessage() {
{{- range .GrpcServices }}
    AsyncCompleteGrpc{{.ServiceName}}{{.Method}}();
{{- end }}
}

`
