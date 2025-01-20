package gen

const AsyncClientHeaderTemplate = `#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/{{.ProtoFileBaseName}}.grpc.pb.h"
#include "proto/common/{{.ProtoFileBaseName}}.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using Grpc{{.ServiceName}}StubPtr = std::unique_ptr<{{.ServiceName}}::Stub>;


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
void {{.ServiceName}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.Request}}& request);


{{- end }}

void Handle{{.ServiceName}}CompletedQueueMessage(entt::registry& registry	); 

void Init{{.ServiceName}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity);
`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"

#include "grpc/generator/{{.GeneratorFileName}}.h"
#include "thread_local/storage.h"


entt::entity GlobalGrpcNodeEntity();

{{- range .GrpcServices }}
struct {{.ServiceName}}{{.Method}}CompleteQueue{
	grpc::CompletionQueue cq;
};
{{- end }}

{{- range .GrpcServices }}

void {{.ServiceName}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.Request}}& request)
{
    Async{{.ServiceName}}{{.Method}}GrpcClientCall* call = new Async{{.ServiceName}}{{.Method}}GrpcClientCall;

    call->response_reader =
        registry.get<Grpc{{.ServiceName}}StubPtr>(nodeEntity)->PrepareAsync{{.Method}}(&call->context, request,
		&registry.get<{{.ServiceName}}{{.Method}}CompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<Async{{.ServiceName}}{{.Method}}GrpcClientCall>&)> Async{{.ServiceName}}{{.Method}}Handler;

void AsyncCompleteGrpc{{.ServiceName}}{{.Method}}(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
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

void Init{{.ServiceName}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
{{- range .GrpcServices }}
	registry.emplace<{{.ServiceName}}{{.Method}}CompleteQueue>(nodeEntity);
{{- end }}
}

void Handle{{.ServiceName}}CompletedQueueMessage(entt::registry& registry) {
{{- range .GrpcServices }}
	{
		auto&& view = registry.view<{{.ServiceName}}{{.Method}}CompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpc{{.ServiceName}}{{.Method}}(completeQueueComp.cq);
		}
	}
{{- end }}
}

`
