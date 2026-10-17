#include "muduo/base/Logging.h"

#include "rank_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetRankCallDeadline 按目标节点类型写入
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

namespace rankpb {
#pragma region ClientPlayerRankGetRankPage
AsyncClientPlayerRankGetRankPageHandlerFunctionType AsyncClientPlayerRankGetRankPageHandler;
AsyncClientPlayerRankGetRankPageFailedHandlerFunctionType AsyncClientPlayerRankGetRankPageFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerRankGetRankPageCall {
    ::rankpb::GetRankPageRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerRankGetRankPageHandler / FailedHandler。
    GrpcDone<::rankpb::GetRankPageResponse> done;
};

void CompleteClientPlayerRankGetRankPage(const ClientPlayerRankGetRankPageCall& call, const GrpcCallOutcome& outcome,
                                                const ::rankpb::GetRankPageResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerRankGetRankPageHandler) {
            AsyncClientPlayerRankGetRankPageHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerRank.GetRankPage reply dropped: AsyncClientPlayerRankGetRankPageHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerRankGetRankPageFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerRankGetRankPageMessageId, "ClientPlayerRank.GetRankPage", outcome, call.sentMetadata};
        AsyncClientPlayerRankGetRankPageFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerRank.GetRankPage failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerRankGetRankPageFailure(std::shared_ptr<ClientPlayerRankGetRankPageCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerRankGetRankPage(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerRankGetRankPageCall(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::rankpb::GetRankPageResponse> done) {
    auto call = std::make_shared<ClientPlayerRankGetRankPageCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerRankGetRankPageFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerRankGetRankPageGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerRankGetRankPageFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerRankGetRankPage(*call, outcome, nullptr);
            return;
        }
        ::rankpb::GetRankPageResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerRank.GetRankPage response";
            CompleteClientPlayerRankGetRankPage(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerRankGetRankPage(*call, outcome, &reply);
    });
}

void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request,
                                            GrpcDone<::rankpb::GetRankPageResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerRankGetRankPageCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerRankGetRankPage(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerRankGetRankPageCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::rankpb::GetRankPageRequest& derived = static_cast<const ::rankpb::GetRankPageRequest&>(message);
    SendClientPlayerRankGetRankPage(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetRankHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerRankGetRankPageHandler = handler;
    (void)handler;
}

void SetRankIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientPlayerRankGetRankPageHandler) {
        AsyncClientPlayerRankGetRankPageHandler = handler;
    }
    (void)handler;
}

void SetRankFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerRankGetRankPageFailedHandler = handler;
    (void)handler;
}

void SetRankIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerRankGetRankPageFailedHandler) {
        AsyncClientPlayerRankGetRankPageFailedHandler = handler;
    }
    (void)handler;
}

void SetRankCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace rankpb
