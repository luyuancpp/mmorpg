package internal

const AsyncClientHeaderTemplate = `#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
{{.GrpcIncludeHeadName}}

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace {{.Package}} {
enum class GrpcMethod {
{{- range $svc := .ServiceInfo }}
    {{- range $method := $svc.MethodInfo }}
    {{ $svc.Service }}_{{ $method.Method }},
    {{- end }}
{{- end }}
};

{{- range $svc := .ServiceInfo }}
using {{ $svc.Service }}StubPtr = std::unique_ptr<{{ $svc.Service }}::Stub>;

{{- range $index, $method := $svc.MethodInfo }}
#pragma region {{ $svc.Service }}{{ $method.Method }}
{{- $enumValue := printf "%s_%s" $svc.Service $method.Method }}
{{ if $method.ClientStreaming }}

struct Async{{ $svc.Service }}{{ $method.Method }}GrpcClient {
    uint32_t type{ static_cast<uint32_t>(GrpcMethod::{{ $enumValue }}) };
    ClientContext context;
    Status status;
    {{ $method.CppResponse }} reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<{{ $method.CppRequest }}, {{ $method.CppResponse }}>> stream;
};

struct {{ $method.RequestName }}Buffer {
    boost::circular_buffer<{{ $method.CppRequest }}> pendingWritesBuffer{200};
};

struct {{ $method.RequestName }}WriteInProgress {
    bool isInProgress{false};
};

{{ else }}

struct Async{{ $svc.Service }}{{ $method.Method }}GrpcClientCall {
    uint32_t type{ static_cast<uint32_t>(GrpcMethod::{{ $enumValue }}) };
    ClientContext context;
    Status status;
    {{ $method.CppResponse }} reply;
    std::unique_ptr<ClientAsyncResponseReader<{{ $method.CppResponse }}>> response_reader;
};

{{ end }}

class {{ $method.CppRequest }};
using Async{{ $svc.Service }}{{ $method.Method }}HandlerFunctionType = std::function<void(const ClientContext&, const {{ $method.CppResponse }}&)>;
extern Async{{ $svc.Service }}{{ $method.Method }}HandlerFunctionType Async{{ $svc.Service }}{{ $method.Method }}Handler;

void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const {{ $method.CppRequest }}& request);
void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const {{ $method.CppRequest }}& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

{{ end }}
{{- end }}


{{- range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Set{{$m.FileBaseNameCamel}}Handler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void Set{{$m.FileBaseNameCamel}}IfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void Init{{$m.FileBaseNameCamel}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity);
void Handle{{$m.FileBaseNameCamel}}CompletedQueueMessage(entt::registry& registry);
void Init{{$m.FileBaseNameCamel}}Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
  {{- end }}
{{- end }}


}// namespace {{.Package}}
`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"
#include "{{.GeneratorGrpcFileName}}.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"

{{ $root := . }}

namespace {{.Package}}{
struct {{.GrpcCompleteQueueName}} {
    grpc::CompletionQueue cq;
};

{{- range .ServiceInfo }}
{{- range .MethodInfo }}
#pragma region {{.Service}}{{.Method}}

using Async{{.Service}}{{.Method}}HandlerFunctionType = std::function<void(const ClientContext&, const {{.CppResponse}}&)>;
Async{{.Service}}{{.Method}}HandlerFunctionType Async{{.Service}}{{.Method}}Handler;
{{ if .ClientStreaming }}
void TryWriteNextNext{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    auto& writeInProgress = registry.get<{{.RequestName}}WriteInProgress>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;

    if (writeInProgress.isInProgress || pendingWritesBuffer.empty()) {
        return;
    }

    auto& client = registry.get<Async{{.Service}}{{.Method}}GrpcClient>(nodeEntity);
    auto& request = pendingWritesBuffer.front();

    writeInProgress.isInProgress = true;
    client.stream->Write(request, (void*)(GrpcOperation::WRITE));
}

void AsyncCompleteGrpc{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;

    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }

    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    auto& client = registry.get<Async{{.Service}}{{.Method}}GrpcClient>(nodeEntity);
    auto& writeInProgress = registry.get<{{.RequestName}}WriteInProgress>(nodeEntity);

    switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))) {
        case GrpcOperation::WRITE: {
            auto& pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;
            if (!pendingWritesBuffer.empty()) {
                pendingWritesBuffer.pop_front();
            }
            writeInProgress.isInProgress = false;
            TryWriteNextNext{{.Service}}{{.Method}}(registry, nodeEntity, cq);
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
            if (Async{{.Service}}{{.Method}}Handler) {
                Async{{.Service}}{{.Method}}Handler(client.context, response);
            }
            client.stream->Read(&response, (void*)GrpcOperation::READ);
            TryWriteNextNext{{.Service}}{{.Method}}(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::INIT: {
            auto& response = registry.get<{{.CppResponse}}>(nodeEntity);
            client.stream->Read(&response, (void*)GrpcOperation::READ);
            TryWriteNextNext{{.Service}}{{.Method}}(registry, nodeEntity, cq);
            break;
        }
        default:
            break;
    }
}
{{ else }}


void AsyncCompleteGrpc{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<Async{{.Service}}{{.Method}}GrpcClientCall> call(
        static_cast<Async{{.Service}}{{.Method}}GrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (Async{{.Service}}{{.Method}}Handler) {
            Async{{.Service}}{{.Method}}Handler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}
{{- end }}

void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.CppRequest}}& request) {
{{ if .ClientStreaming }}
    auto& cq = registry.get< {{ $root.GrpcCompleteQueueName }}>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNext{{.Service}}{{.Method}}(registry, nodeEntity, cq);
{{ else }}
    Async{{.Service}}{{.Method}}GrpcClientCall* call = new Async{{.Service}}{{.Method}}GrpcClientCall;
    call->response_reader = registry
        .get<{{.Service}}StubPtr>(nodeEntity)
        ->PrepareAsync{{.Method}}(&call->context, request,
                                  &registry.get< {{ $root.GrpcCompleteQueueName }}>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
{{ end }}
}

void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.CppRequest}}& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
{{ if .ClientStreaming }}
    auto& cq = registry.get< {{ $root.GrpcCompleteQueueName }}>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNext{{.Service}}{{.Method}}(registry, nodeEntity, cq);
{{ else }}
    Async{{.Service}}{{.Method}}GrpcClientCall* call = new Async{{.Service}}{{.Method}}GrpcClientCall;

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<{{.Service}}StubPtr>(nodeEntity)
        ->PrepareAsync{{.Method}}(&call->context, request,
                                  &registry.get< {{ $root.GrpcCompleteQueueName }} >(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
{{ end }}
}

void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const  google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
	const {{.CppRequest}}& derived = static_cast<const {{.CppRequest}}&>(message);
	Send{{.Service}}{{.Method}}(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion


{{end }}
{{- end }}

{{range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Init{{$m.FileBaseNameCamel}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
  {{- end }}
{{- end -}}
    registry.emplace< {{ $root.GrpcCompleteQueueName }} >(nodeEntity);
{{- range .ServiceInfo }}
{{- range .MethodInfo }}
{{ if .ClientStreaming }}
    {
        auto& client = registry.emplace<Async{{.Service}}{{.Method}}GrpcClient>(nodeEntity);
        registry.emplace<{{.RequestName}}Buffer>(nodeEntity);
        registry.emplace<{{.RequestName}}WriteInProgress>(nodeEntity);
        registry.emplace<{{.CppResponse}}>(nodeEntity);
        registry.emplace<{{.CppRequest}}>(nodeEntity);

        client.stream = registry
            .get<{{.Service}}StubPtr>(nodeEntity)
            ->Async{{.Method}}(&client.context,
                               &registry.get< {{ $root.GrpcCompleteQueueName }}>(nodeEntity).cq,
                               (void*)(GrpcOperation::INIT));
    }
{{ end }}
{{- end -}}
{{- end -}}
}

{{range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Handle{{$m.FileBaseNameCamel}}CompletedQueueMessage(entt::registry& registry) {
  {{- end }}
{{- end -}}
{{- range .ServiceInfo }}
{{- range .MethodInfo }}
    {
        auto&& view = registry.view< {{ $root.GrpcCompleteQueueName }}>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpc{{.Service}}{{.Method}}(registry, e, completeQueueComp.cq);
        }
    }
{{- end -}}
{{- end }}
}

{{range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Set{{$m.FileBaseNameCamel}}Handler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){
  {{- end }}
{{- end -}}
{{range $index, $m := .ServiceInfo }}
{{- range $m.MethodInfo }}
   Async{{$m.Service}}{{.Method}}Handler = handler;
{{- end -}}
{{- end }}
}

{{range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Set{{$m.FileBaseNameCamel}}IfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler){
  {{- end }}
{{- end -}}
{{range $index, $m := .ServiceInfo }}
{{- range $m.MethodInfo }}
	if (!Async{{$m.Service}}{{.Method}}Handler){
   		Async{{$m.Service}}{{.Method}}Handler = handler;
	}
{{- end -}}
{{- end }}
}

{{range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Init{{$m.FileBaseNameCamel}}Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity){
  {{- end }}
{{- end -}}
{{- range .ServiceInfo }}
	registry.emplace<{{.Service}}StubPtr>(nodeEntity, {{.Service}}::NewStub(channel));
{{- end }}
}

}// namespace {{.Package}}
`
