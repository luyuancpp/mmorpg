package internal

const AsyncClientHeaderTemplate = `#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
{{.GrpcIncludeHeadName}}
{{ range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
{{$m.ServiceInfoIncludeName}}
  {{- end }}
{{- end }}

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace {{.Package}} {
{{- range $svc := .ServiceInfo }}
using {{ $svc.Service }}StubPtr = std::unique_ptr<{{ $svc.Service }}::Stub>;

{{- range $method := $svc.MethodInfo }}
#pragma region {{ $svc.Service }}{{ $method.Method }}
{{ if $method.ClientStreaming }}

struct Async{{ $svc.Service }}{{ $method.Method }}GrpcClient {
    uint32_t messageId{ {{ $svc.Service }}{{ $method.Method }}MessageId };
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

struct Async{{ $svc.Service }}{{ $method.Method }}GrpcClient {
    uint32_t messageId{ {{ $svc.Service }}{{ $method.Method }}MessageId };
    ClientContext context;
    Status status;
    {{ $method.CppResponse }} reply;
    std::unique_ptr<ClientAsyncResponseReader<{{ $method.CppResponse }}>> response_reader;
};

{{ end }}

class {{ $method.CppRequest }};
using Async{{ $svc.Service }}{{ $method.Method }}HandlerFunctionType =
    std::function<void(const ClientContext&, const {{ $method.CppResponse }}&)>;
extern Async{{ $svc.Service }}{{ $method.Method }}HandlerFunctionType Async{{ $svc.Service }}{{ $method.Method }}Handler;

void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const {{ $method.CppRequest }}& request);
void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const {{ $method.CppRequest }}& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

{{- end }}
{{- end }}

{{- range $index, $svc := .ServiceInfo }}
  {{- if eq $index 0 }}
void Set{{$svc.FileBaseNameCamel}}Handler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void Set{{$svc.FileBaseNameCamel}}IfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void Handle{{$svc.FileBaseNameCamel}}CompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void Init{{$svc.FileBaseNameCamel}}GrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);
  {{- end }}
{{- end }}

}// namespace {{.Package}}

`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"
{{ $root := . }}

#include "{{.GeneratorGrpcFileName}}.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {{.Package}} {
struct {{.GrpcCompleteQueueName}} {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> tagPool;

{{- range $svc := .ServiceInfo }}
{{- range $method := $svc.MethodInfo }}
#pragma region {{ $svc.Service }}{{ $method.Method }}
boost::object_pool<Async{{ $svc.Service }}{{ $method.Method }}GrpcClient> {{ $svc.Service }}{{ $method.Method }}Pool;
using Async{{ $svc.Service }}{{ $method.Method }}HandlerFunctionType =
    std::function<void(const ClientContext&, const {{ $method.CppResponse }}&)>;
Async{{ $svc.Service }}{{ $method.Method }}HandlerFunctionType Async{{ $svc.Service }}{{ $method.Method }}Handler;
{{ if $method.ClientStreaming }}

void TryWriteNextNext{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    auto& writeInProgress = registry.get<{{ $method.RequestName }}WriteInProgress>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<{{ $method.RequestName }}Buffer>(nodeEntity).pendingWritesBuffer;

    if (writeInProgress.isInProgress || pendingWritesBuffer.empty()) {
        return;
    }

    auto& client = registry.get<Async{{ $svc.Service }}{{ $method.Method }}GrpcClient>(nodeEntity);
    auto& request = pendingWritesBuffer.front();

    writeInProgress.isInProgress = true;
    GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId,  (void*)GrpcOperation::WRITE));
    client.stream->Write(request, (void*)(got_tag));
}
void AsyncCompleteGrpc{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto& client = registry.get<Async{{ $svc.Service }}{{ $method.Method }}GrpcClient>(nodeEntity);
    auto& writeInProgress = registry.get<{{ $method.RequestName }}WriteInProgress>(nodeEntity);

    switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))) {
        case GrpcOperation::WRITE: {
            auto& pendingWritesBuffer = registry.get<{{ $method.RequestName }}Buffer>(nodeEntity).pendingWritesBuffer;
            if (!pendingWritesBuffer.empty()) {
                pendingWritesBuffer.pop_front();
            }
            writeInProgress.isInProgress = false;
            TryWriteNextNext{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::WRITES_DONE: {
            GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId,  (void*)GrpcOperation::READ));
            client.stream->Finish(&client.status, (void*)(got_tag));
            break;
        }
        case GrpcOperation::FINISH:
            cq.Shutdown();
            break;
        case GrpcOperation::READ: {
            auto& response = registry.get<{{ $method.CppResponse }}>(nodeEntity);
            if (Async{{ $svc.Service }}{{ $method.Method }}Handler) {
                Async{{ $svc.Service }}{{ $method.Method }}Handler(client.context, response);
            }
            GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId, (void*)GrpcOperation::READ));
            client.stream->Read(&response, (void*)got_tag);
            TryWriteNextNext{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::INIT: {
            GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId, (void*)GrpcOperation::READ));
            auto& response = registry.get<{{ $method.CppResponse }}>(nodeEntity);
            client.stream->Read(&response, (void*)got_tag);
            TryWriteNextNext{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, cq);
            break;
        }
        default:
            break;
    }
}
{{ else }}
void AsyncCompleteGrpc{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<Async{{ $svc.Service }}{{ $method.Method }}GrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (Async{{ $svc.Service }}{{ $method.Method }}Handler) {
            Async{{ $svc.Service }}{{ $method.Method }}Handler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	{{ $svc.Service }}{{ $method.Method }}Pool.destroy(call);
}

{{ end }}

void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const {{ $method.CppRequest }}& request) {
{{ if $method.ClientStreaming }}
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<{{ $method.RequestName }}Buffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNext{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, cq);
{{ else }}
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call({{ $svc.Service }}{{ $method.Method }}Pool.construct());
    call->response_reader = registry
        .get<{{ $svc.Service }}StubPtr>(nodeEntity)
        ->PrepareAsync{{ $method.Method }}(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
{{ end }}
}


void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const {{ $method.CppRequest }}& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
{{ if $method.ClientStreaming }}
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<{{ $method.RequestName }}Buffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNext{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, cq);
{{ else }}
    auto call({{ $svc.Service }}{{ $method.Method }}Pool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<{{ $svc.Service }}StubPtr>(nodeEntity)
        ->PrepareAsync{{ $method.Method }}(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
{{ end }}
}

void Send{{ $svc.Service }}{{ $method.Method }}(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const {{ $method.CppRequest }}& derived = static_cast<const {{ $method.CppRequest }}&>(message);
    Send{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
{{- end }}
{{- end }}

{{ range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Handle{{ $m.FileBaseNameCamel }}CompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
  {{- end -}}
{{- end }}
        switch (grpcTag->messageId) {
{{- range $svc := .ServiceInfo }}
{{- range $method := $svc.MethodInfo }}
        case {{ $svc.Service }}{{ $method.Method }}MessageId:
            AsyncCompleteGrpc{{ $svc.Service }}{{ $method.Method }}(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
{{- end }}
{{- end }}
        default:
            break;
        }
}


{{ range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Set{{ $m.FileBaseNameCamel }}Handler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {
  {{- end }}
{{ end }}
{{- range $svc := .ServiceInfo }}
{{- range $method := $svc.MethodInfo }}
    Async{{ $svc.Service }}{{ $method.Method }}Handler = handler;
{{- end }}
{{- end }}
}

{{ range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Set{{ $m.FileBaseNameCamel }}IfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {
  {{- end }}
{{ end }}
{{- range $svc := .ServiceInfo }}
{{- range $method := $svc.MethodInfo }}
    if (!Async{{ $svc.Service }}{{ $method.Method }}Handler) {
        Async{{ $svc.Service }}{{ $method.Method }}Handler = handler;
    }
{{- end }}
{{- end }}
}

{{ range $index, $m := .ServiceInfo }}
  {{- if eq $index 0 }}
void Init{{ $m.FileBaseNameCamel }}GrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {
  {{- end }}
{{ end }}
{{- range $svc := .ServiceInfo }}
    registry.emplace<{{ $svc.Service }}StubPtr>(nodeEntity, {{ $svc.Service }}::NewStub(channel));
{{- end }}


{{- range $svc := .ServiceInfo -}}
{{- range $method := $svc.MethodInfo -}}
{{ if $method.ClientStreaming }}
    {
        GrpcTag* got_tag(tagPool.construct({{ $svc.Service }}{{ $method.Method }}MessageId, (void*)GrpcOperation::INIT));

        auto& client = registry.emplace<Async{{ $svc.Service }}{{ $method.Method }}GrpcClient>(nodeEntity);
        registry.emplace<{{ $method.RequestName }}Buffer>(nodeEntity);
        registry.emplace<{{ $method.RequestName }}WriteInProgress>(nodeEntity);
        registry.emplace<{{ $method.CppResponse }}>(nodeEntity);
        registry.emplace<{{ $method.CppRequest }}>(nodeEntity);

        client.stream = registry
            .get<{{ $svc.Service }}StubPtr>(nodeEntity)
            ->Async{{ $method.Method }}(&client.context,
                                        &registry.get<grpc::CompletionQueue>(nodeEntity),
                                        (void*)(got_tag));
    }
{{- end -}}
{{- end -}}
{{ end }}

}


}// namespace {{.Package}}
`
