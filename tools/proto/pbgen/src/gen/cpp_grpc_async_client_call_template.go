package gen

const AsyncClientHeaderTemplate = `#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

{{.GrpcIncludeHeadName}}

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using Grpc{{.ServiceFullNameWithNoColon}}StubPtr = std::unique_ptr<{{.Package}}::{{.Service}}::Stub>;


{{- range .GrpcServices }}
class Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall
{
public:
    ClientContext context;
    Status status;
    {{.CppResponse}} reply;
    std::unique_ptr<ClientAsyncResponseReader<  {{.CppResponse}}>> response_reader;
};

class {{.CppRequest}};
void Send{{.ServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  {{.CppRequest}}& request);

using Async{{.ServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall>&)>;

extern Async{{.ServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.ServiceFullNameWithNoColon}}{{.Method}}Handler;;

{{- end }}

void Handle{{.ServiceFullNameWithNoColon}}CompletedQueueMessage(entt::registry& registry	); 

void Init{{.ServiceFullNameWithNoColon}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity);


`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"

#include "grpc/generator/{{.GeneratorFileName}}.h"
#include "thread_local/storage.h"


{{- range .GrpcServices }}
struct {{.ServiceFullNameWithNoColon}}{{.Method}}CompleteQueue{
	grpc::CompletionQueue cq;
};
{{- end }}

{{- range .GrpcServices }}

void Send{{.ServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  {{.CppRequest}}& request)
{
    Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall* call = new Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall;

    call->response_reader =
        registry.get<Grpc{{.ServiceFullNameWithNoColon}}StubPtr>(nodeEntity)->PrepareAsync{{.Method}}(&call->context, request,
		&registry.get<{{.ServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using Async{{.ServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall>&)>;
Async{{.ServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.ServiceFullNameWithNoColon}}{{.Method}}Handler;

void AsyncCompleteGrpc{{.ServiceFullNameWithNoColon}}{{.Method}}(grpc::CompletionQueue& cq)
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

    std::unique_ptr<Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall> call(static_cast<Async{{.ServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(Async{{.ServiceFullNameWithNoColon}}{{.Method}}Handler){
			Async{{.ServiceFullNameWithNoColon}}{{.Method}}Handler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
{{- end }}

void Init{{.ServiceFullNameWithNoColon}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
{{- range .GrpcServices }}
	registry.emplace<{{.ServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity);
{{- end }}
}

void Handle{{.ServiceFullNameWithNoColon}}CompletedQueueMessage(entt::registry& registry) {
{{- range .GrpcServices }}
	{
		auto&& view = registry.view<{{.ServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpc{{.ServiceFullNameWithNoColon}}{{.Method}}(completeQueueComp.cq);
		}
	}
{{- end }}
}

`
