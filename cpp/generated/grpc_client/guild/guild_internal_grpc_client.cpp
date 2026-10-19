#include "muduo/base/Logging.h"

#include "guild_internal_grpc_client.h"
#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include "core/network/grpc_client_runtime.h"
#include "core/utils/encode/base64.h"
#include "muduo_grpc/channel.h"

namespace {
// 每个 proto 文件独立的进程级 deadline;启动时按目标节点类型写入。
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

namespace guildpb {
#pragma region GuildInternalListAppliedAssetOpsSince
namespace {
void QueueLocalGuildInternalListAppliedAssetOpsSinceFailure(GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartGuildInternalListAppliedAssetOpsSinceCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC GuildInternal.ListAppliedAssetOpsSince requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildInternalListAppliedAssetOpsSinceGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalGuildInternalListAppliedAssetOpsSinceFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildInternalListAppliedAssetOpsSinceFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::guildpb::ListAppliedAssetOpsSinceResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildInternal.ListAppliedAssetOpsSince response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request,
                                            GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done, const GrpcCallOptions& options) {
    StartGuildInternalListAppliedAssetOpsSinceCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildInternalListAppliedAssetOpsSinceErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC GuildInternal.ListAppliedAssetOpsSince requires a done callback";
    }
    if (message.GetDescriptor() != ::guildpb::ListAppliedAssetOpsSinceRequest::descriptor()) {
        LOG_ERROR << "gRPC GuildInternal.ListAppliedAssetOpsSince: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::guildpb::ListAppliedAssetOpsSinceRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC GuildInternal.ListAppliedAssetOpsSince: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartGuildInternalListAppliedAssetOpsSinceCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::guildpb::ListAppliedAssetOpsSinceResponse* reply) { done(outcome, reply); });
}
#pragma endregion

void SetGuildInternalCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace guildpb
