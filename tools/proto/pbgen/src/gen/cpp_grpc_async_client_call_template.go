package gen

const AsyncClientHeaderTemplate = `#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

{{.GrpcIncludeHeadName}}

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

{{- range .ServiceInfo }}
using Grpc{{.GetServiceFullNameWithNoColon}}StubPtr = std::unique_ptr<{{.Package}}::{{.Service}}::Stub>;
{{- range .MethodInfo }}
{{if .ClientStreaming}}
class Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClient
{
public:
    ClientContext context;
    Status status;
    {{.CppResponse}} reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<{{.CppRequest}},  {{.CppResponse}}>> stream;
};


using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const {{.CppResponse}}&)>;

extern Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;


{{else}}
class Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall
{
public:
    ClientContext context;
    Status status;
    {{.CppResponse}} reply;
	std::unique_ptr<ClientAsyncResponseReader<{{.CppResponse}}>> response_reader;
};

using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall>&)>;

extern Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;
{{end}}

class {{.CppRequest}};
void Send{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  {{.CppRequest}}& request);

void Handle{{.GetServiceFullNameWithNoColon}}CompletedQueueMessage(entt::registry& registry	); 
void Init{{.GetServiceFullNameWithNoColon}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity);

{{- end }}
{{- end }}
`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"

#include "{{.GeneratorGrpcFileName}}.h"
#include "thread_local/storage.h"

static uint32_t GRPC_WRITE_TAG = 1;
static uint32_t GRPC_READ_TAG = 2;
static void* P_GRPC_WRITE_TAG = static_cast<void*>(&GRPC_WRITE_TAG);
static void* P_GRPC_READ_TAG = static_cast<void*>(&GRPC_READ_TAG);


{{- range .ServiceInfo }}
{{- range .MethodInfo }}
struct {{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue{
	grpc::CompletionQueue cq;
};



{{if .ClientStreaming}}
using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const {{.CppResponse}}&)>;
Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;

void AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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

	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

	auto& client = registry.get<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClient>(nodeEntity);

	if (got_tag == P_GRPC_WRITE_TAG) {
	} else {
		{{.CppResponse}} response;
		client.stream->Read(&response, P_GRPC_READ_TAG);

		if(Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler){
			Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler(response);
		}
	}
}

{{else}}
using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall>&)>;
Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;

void AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
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
{{end}}

void Send{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  {{.CppRequest}}& request)
{
{{if .ClientStreaming}}
	auto& client = registry.get<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClient>(nodeEntity);
	client.stream->Write(request, static_cast<void*>(P_GRPC_WRITE_TAG));
	auto& cq = registry.get<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq;
	AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, nodeEntity, cq);
{{else}}
    Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall* call = new Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClientCall;
    call->response_reader =
        registry.get<Grpc{{.GetServiceFullNameWithNoColon}}StubPtr>(nodeEntity)->PrepareAsync{{.Method}}(&call->context, request,
		&registry.get<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq);

    	call->response_reader->StartCall();

    	call->response_reader->Finish(&call->reply, &call->status, (void*)call);
{{end}}
}

{{- end }}
{{- end }}

{{- range .ServiceInfo }}
void Init{{.GetServiceFullNameWithNoColon}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
{{- range .MethodInfo }}
	registry.emplace<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity);
{{if .ClientStreaming}}
	{
		auto& client = registry.emplace<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClient>(nodeEntity);
		client.stream =
			registry.get<Grpc{{.GetServiceFullNameWithNoColon}}StubPtr>(nodeEntity)->PrepareAsync{{.Method}}(&client.context, 
			&registry.get<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq);
		client.stream->StartCall(static_cast<void*>(&client));
	}
{{end}}
{{- end }}
}
{{- end }}

{{- range .ServiceInfo }}
void Handle{{.GetServiceFullNameWithNoColon}}CompletedQueueMessage(entt::registry& registry) {
{{- range .MethodInfo }}
	{
		auto&& view = registry.view<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, e, completeQueueComp.cq);
		}
	}
{{- end }}
}
{{- end }}
`
