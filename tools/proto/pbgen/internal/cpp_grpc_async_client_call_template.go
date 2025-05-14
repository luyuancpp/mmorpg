package internal

const AsyncClientHeaderTemplate = `#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
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

struct  {{.RequestName}}Buffer{
	boost::circular_buffer<{{.CppRequest}}> pendingWritesBuffer{200};
};

struct {{.RequestName}}WriteInProgress{ bool isInProgress{false};};

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
#include "proto/logic/constants/etcd_grpc.pb.h"

{{- range .ServiceInfo }}
{{- range .MethodInfo }}

struct {{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue{
	grpc::CompletionQueue cq;
};

{{if .ClientStreaming}}
using Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType = std::function<void(const {{.CppResponse}}&)>;
Async{{.GetServiceFullNameWithNoColon}}{{.Method}}HandlerFunctionType  Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler;

void TryWriteNextNext{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
{
	auto&  writeInProgress = registry.get<{{.RequestName}}WriteInProgress>(nodeEntity);
	auto&  pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;

	if (writeInProgress.isInProgress){
		return;
	}
	if (pendingWritesBuffer.empty()){
		return;
	}

	auto& client = registry.get<Async{{.GetServiceFullNameWithNoColon}}{{.Method}}GrpcClient>(nodeEntity);
	auto& request = pendingWritesBuffer.front();
	writeInProgress.isInProgress = true;
	client.stream->Write(request,  (void*)(GrpcOperation::WRITE));		
}

void AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq)
{
    void* got_tag = nullptr;
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
	auto& writeInProgress = registry.get<{{.RequestName}}WriteInProgress>(nodeEntity);


	switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))) {
	case GrpcOperation::WRITE: {
		auto& pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;
		if (!pendingWritesBuffer.empty()) {
			pendingWritesBuffer.pop_front();
		}

		writeInProgress.isInProgress = false;

		// 写完之后尝试继续写（而不是触发 Read）
		TryWriteNextNext{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, nodeEntity, cq);  // 写
		break;
	}

	case GrpcOperation::WRITES_DONE:
		client.stream->Finish(&client.status, (void*)(GrpcOperation::FINISH));
		break;
	case GrpcOperation::FINISH:
		cq.Shutdown();
		break;
	case GrpcOperation::READ: {
		auto& response = registry.get<{{.CppResponse}}>(nodeEntity);

		if(Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler){
			Async{{.GetServiceFullNameWithNoColon}}{{.Method}}Handler(response);
		}

		client.stream->Read(&response, (void*)GrpcOperation::READ);  // 持续读
		TryWriteNextNext{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, nodeEntity, cq);  // 
		break;
	}

	case GrpcOperation::INIT: {
		auto& response = registry.get<{{.CppResponse}}>(nodeEntity);
		client.stream->Read(&response, (void*)GrpcOperation::READ);  // 第一次 read

		// 如果 buffer 里已经有请求，也可以尝试触发 write
		TryWriteNextNext{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, nodeEntity, cq);
		break;
	}
	default:
		break;
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
	auto& cq = registry.get<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq;
	auto&  pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;
	pendingWritesBuffer.push_back(request);
	TryWriteNextNext{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, nodeEntity, cq);
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
		registry.emplace<{{.RequestName}}Buffer>(nodeEntity);
		registry.emplace<{{.RequestName}}WriteInProgress>(nodeEntity);
		registry.emplace<{{.CppResponse}}>(nodeEntity);
		registry.emplace<{{.CppRequest}}>(nodeEntity);
		client.stream =
			registry.get<Grpc{{.GetServiceFullNameWithNoColon}}StubPtr>(nodeEntity)->Async{{.Method}}(&client.context, 
				&registry.get<{{.GetServiceFullNameWithNoColon}}{{.Method}}CompleteQueue>(nodeEntity).cq, (void*)(GrpcOperation::INIT));
		
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
		for (auto&& [e, completeQueueComp] : view.each()) 	{
			AsyncCompleteGrpc{{.GetServiceFullNameWithNoColon}}{{.Method}}(registry, e, completeQueueComp.cq);
		}
	}
{{- end }}
}
{{- end }}
`
