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
{{- range .ServiceInfo }}
using {{.Service}}StubPtr = std::unique_ptr<{{.Service}}::Stub>;
{{- range .MethodInfo }}
#pragma region {{.Service}}{{.Method}}
{{if .ClientStreaming}}
class Async{{.Service}}{{.Method}}GrpcClient {
public:
    ClientContext context;
    Status status;
    {{.CppResponse}} reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<{{.CppRequest}}, {{.CppResponse}}>> stream;
};

struct {{.RequestName}}Buffer {
    boost::circular_buffer<{{.CppRequest}}> pendingWritesBuffer{200};
};

struct {{.RequestName}}WriteInProgress {
    bool isInProgress{false};
};

using Async{{.Service}}{{.Method}}HandlerFunctionType = std::function<void(const {{.CppResponse}}&)>;
extern Async{{.Service}}{{.Method}}HandlerFunctionType Async{{.Service}}{{.Method}}Handler;
{{else}}
class Async{{.Service}}{{.Method}}GrpcClientCall {
public:
    ClientContext context;
    Status status;
    {{.CppResponse}} reply;
    std::unique_ptr<ClientAsyncResponseReader<{{.CppResponse}}>> response_reader;
};

using Async{{.Service}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.Service}}{{.Method}}GrpcClientCall>&)>;
extern Async{{.Service}}{{.Method}}HandlerFunctionType Async{{.Service}}{{.Method}}Handler;
{{end}}

class {{.CppRequest}};

void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.CppRequest}}& request);
void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.CppRequest}}& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void Handle{{.Service}}CompletedQueueMessage(entt::registry& registry);
void Init{{.Service}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion

{{ end }}
{{- end }}

}// namespace {{.Package}}
`

const AsyncClientCppHandleTemplate = `#include "muduo/base/Logging.h"
#include "{{.GeneratorGrpcFileName}}.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "util/base64.h"

namespace {{.Package}}{
{{- range .ServiceInfo }}
{{- range .MethodInfo }}
#pragma region {{.Service}}{{.Method}}
struct {{.Service}}{{.Method}}CompleteQueue {
    grpc::CompletionQueue cq;
};
{{ if .ClientStreaming }}
using Async{{.Service}}{{.Method}}HandlerFunctionType = std::function<void(const {{.CppResponse}}&)>;
Async{{.Service}}{{.Method}}HandlerFunctionType Async{{.Service}}{{.Method}}Handler;
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
                Async{{.Service}}{{.Method}}Handler(response);
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
using Async{{.Service}}{{.Method}}HandlerFunctionType = std::function<void(const std::unique_ptr<Async{{.Service}}{{.Method}}GrpcClientCall>&)>;
Async{{.Service}}{{.Method}}HandlerFunctionType Async{{.Service}}{{.Method}}Handler;

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
            Async{{.Service}}{{.Method}}Handler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}
{{- end }}

void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.CppRequest}}& request) {
{{ if .ClientStreaming }}
    auto& cq = registry.get<{{.Service}}{{.Method}}CompleteQueue>(nodeEntity).cq;
    auto& pendingWritesBuffer = registry.get<{{.RequestName}}Buffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNext{{.Service}}{{.Method}}(registry, nodeEntity, cq);
{{ else }}
    Async{{.Service}}{{.Method}}GrpcClientCall* call = new Async{{.Service}}{{.Method}}GrpcClientCall;
    call->response_reader = registry
        .get<{{.Service}}StubPtr>(nodeEntity)
        ->PrepareAsync{{.Method}}(&call->context, request,
                                  &registry.get<{{.Service}}{{.Method}}CompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
{{ end }}
}

void Send{{.Service}}{{.Method}}(entt::registry& registry, entt::entity nodeEntity, const {{.CppRequest}}& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
{{ if .ClientStreaming }}
    auto& cq = registry.get<{{.Service}}{{.Method}}CompleteQueue>(nodeEntity).cq;
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
                                  &registry.get<{{.Service}}{{.Method}}CompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
{{ end }}
}
#pragma endregion


{{end }}
{{- end }}

{{- range .ServiceInfo }}
void Init{{.Service}}CompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
{{- range .MethodInfo }}
    registry.emplace<{{.Service}}{{.Method}}CompleteQueue>(nodeEntity);
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
                               &registry.get<{{.Service}}{{.Method}}CompleteQueue>(nodeEntity).cq,
                               (void*)(GrpcOperation::INIT));
    }
{{ end }}
{{- end -}}
}
{{- end -}}

{{- range .ServiceInfo }}
void Handle{{.Service}}CompletedQueueMessage(entt::registry& registry) {
{{- range .MethodInfo }}
    {
        auto&& view = registry.view<{{.Service}}{{.Method}}CompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpc{{.Service}}{{.Method}}(registry, e, completeQueueComp.cq);
        }
    }
{{- end -}}
}
{{- end -}}

}// namespace {{.Package}}
`
