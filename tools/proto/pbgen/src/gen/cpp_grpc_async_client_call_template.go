package gen

const AsyncClientHeaderTemplate = `#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

{{.GrpcIncludeHeadName}}

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;


{{- range .GrpcServices }}
using Grpc{{.GetServiceFullNameWithNoColon}}StubPtr = std::unique_ptr<{{.Package}}::{{.Service}}::Stub>;
class Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall
{
public:
    ClientContext context;
    Status status;
    {{.CppResponse}} reply;
    std::unique_ptr<ClientAsyncResponseReader<  {{.CppResponse}}>> response_reader;
};

class {{.CppRequest}};
void Send{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  {{.CppRequest}}& request);

using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall>&)>;

extern Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;;

{{- end }}

void Handle{{.GetServiceFullNameWithNoColon}}CompletedQueueMessage(entt::registry& registry	); 

void Init{{.GetServiceFullNameWithNoColon}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity);


`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"

#include "grpc/generator/{{.GeneratorFileName}}.h"
#include "thread_local/storage.h"


{{- range .GrpcServices }}
struct {{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue{
	grpc::CompletionQueue cq;
};
{{- end }}

{{- range .GrpcServices }}

void Send{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  {{.CppRequest}}& request)
{
    Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall* call = new Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall;

    call->response_reader =
        registry.get<Grpc{{.GetServiceFullNameWithNoColon}}StubPtr>(nodeEntity)->PrepareAsync{{.Method}}(&call->context, request,
		&registry.get<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall>&)>;
Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;

void AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(grpc::CompletionQueue& cq)
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

    std::unique_ptr<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall> call(static_cast<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler){
			Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}
{{- end }}

void Init{{.GetServiceFullNameWithNoColon}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
{{- range .GrpcServices }}
	registry.emplace<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity);
{{- end }}
}

void Handle{{.GetServiceFullNameWithNoColon}}CompletedQueueMessage(entt::registry& registry) {
{{- range .GrpcServices }}
	{
		auto&& view = registry.view<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(completeQueueComp.cq);
		}
	}
{{- end }}
}

`
