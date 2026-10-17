#include "muduo/base/Logging.h"

#include "client_rpc_router_grpc_client.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include "core/network/grpc_client_runtime.h"
#include "core/utils/encode/base64.h"
#include "muduo_grpc/channel.h"

namespace {
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetClientRpcRouterCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::milliseconds DefaultCallDeadline() {
    return std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}

GrpcCallOutcome LocalFailure(uint32_t code, std::string message) {
    GrpcCallOutcome outcome;
    outcome.code = code;
    outcome.message = std::move(message);
    return outcome;
}
}

namespace client_rpc_router {
#pragma region ClientRpcRouterForward
AsyncClientRpcRouterForwardHandlerFunctionType AsyncClientRpcRouterForwardHandler;
AsyncClientRpcRouterForwardFailedHandlerFunctionType AsyncClientRpcRouterForwardFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientRpcRouterForwardCall {
    ::client_rpc_router::ForwardRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientRpcRouterForwardHandler / FailedHandler。
    GrpcDone<::MessageContent> done;
};

void CompleteClientRpcRouterForward(const ClientRpcRouterForwardCall& call, const GrpcCallOutcome& outcome,
                                                const ::MessageContent* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientRpcRouterForwardHandler) {
            AsyncClientRpcRouterForwardHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientRpcRouter.Forward reply dropped: AsyncClientRpcRouterForwardHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientRpcRouterForwardFailedHandler) {
        const GrpcCallFailure failure{ ClientRpcRouterForwardMessageId, "ClientRpcRouter.Forward", outcome, call.sentMetadata};
        AsyncClientRpcRouterForwardFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientRpcRouter.Forward failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientRpcRouterForwardFailure(std::shared_ptr<ClientRpcRouterForwardCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientRpcRouterForward(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientRpcRouterForwardCall(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::MessageContent> done) {
    auto call = std::make_shared<ClientRpcRouterForwardCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientRpcRouterForwardFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientRpcRouterForwardGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientRpcRouterForwardFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    rpc.metadata.reserve(call->sentMetadata.size());
    for (const auto& [key, value] : call->sentMetadata) {
        // 应用层这一层 Base64 不能省:线上字节与改前一致,Go 侧按它解码、按原样回显(§4.2)。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [call](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            CompleteClientRpcRouterForward(*call, outcome, nullptr);
            return;
        }
        ::MessageContent reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientRpcRouter.Forward response";
            CompleteClientRpcRouterForward(*call, outcome, nullptr);
            return;
        }
        CompleteClientRpcRouterForward(*call, outcome, &reply);
    });
}

void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request,
                                            GrpcDone<::MessageContent> done, const GrpcCallOptions& options) {
    StartClientRpcRouterForwardCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientRpcRouterForward(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientRpcRouterForwardCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::client_rpc_router::ForwardRequest& derived = static_cast<const ::client_rpc_router::ForwardRequest&>(message);
    SendClientRpcRouterForward(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetClientRpcRouterHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientRpcRouterForwardHandler = handler;
    (void)handler;
}

void SetClientRpcRouterIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientRpcRouterForwardHandler) {
        AsyncClientRpcRouterForwardHandler = handler;
    }
    (void)handler;
}

void SetClientRpcRouterFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientRpcRouterForwardFailedHandler = handler;
    (void)handler;
}

void SetClientRpcRouterIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientRpcRouterForwardFailedHandler) {
        AsyncClientRpcRouterForwardFailedHandler = handler;
    }
    (void)handler;
}

void SetClientRpcRouterCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace client_rpc_router
