#include "muduo/base/Logging.h"

#include "guild_internal_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetGuildInternalCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace guildpb {
struct GuildInternalCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region GuildInternalListAppliedAssetOpsSince
boost::object_pool<AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient> GuildInternalListAppliedAssetOpsSincePool;
using AsyncGuildInternalListAppliedAssetOpsSinceHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ListAppliedAssetOpsSinceResponse&)>;
AsyncGuildInternalListAppliedAssetOpsSinceHandlerFunctionType AsyncGuildInternalListAppliedAssetOpsSinceHandler;
AsyncGuildInternalListAppliedAssetOpsSinceFailedHandlerFunctionType AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler;

void CompleteGuildInternalListAppliedAssetOpsSince(AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildInternalListAppliedAssetOpsSincePool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildInternalListAppliedAssetOpsSinceHandler) {
            AsyncGuildInternalListAppliedAssetOpsSinceHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildInternal.ListAppliedAssetOpsSince reply dropped: AsyncGuildInternalListAppliedAssetOpsSinceHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "GuildInternal.ListAppliedAssetOpsSince", call->context, call->status, call->sentMetadata};
        AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildInternal.ListAppliedAssetOpsSince failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildInternalListAppliedAssetOpsSince(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildInternalListAppliedAssetOpsSince(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildInternalListAppliedAssetOpsSinceCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done) {
    auto call(GuildInternalListAppliedAssetOpsSincePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildInternalStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildInternalListAppliedAssetOpsSince(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListAppliedAssetOpsSince(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildInternalListAppliedAssetOpsSinceMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request,
                                            GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done, const GrpcCallOptions& options) {
    StartGuildInternalListAppliedAssetOpsSinceCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildInternalListAppliedAssetOpsSince(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildInternalListAppliedAssetOpsSinceCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ListAppliedAssetOpsSinceRequest& derived = static_cast<const ::guildpb::ListAppliedAssetOpsSinceRequest&>(message);
    SendGuildInternalListAppliedAssetOpsSince(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleGuildInternalCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case GuildInternalListAppliedAssetOpsSinceMessageId:
            AsyncCompleteGrpcGuildInternalListAppliedAssetOpsSince(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetGuildInternalHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncGuildInternalListAppliedAssetOpsSinceHandler = handler;
}

void SetGuildInternalIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncGuildInternalListAppliedAssetOpsSinceHandler) {
        AsyncGuildInternalListAppliedAssetOpsSinceHandler = handler;
    }
}

void SetGuildInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler = handler;
}

void SetGuildInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler) {
        AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler = handler;
    }
}

void SetGuildInternalCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitGuildInternalGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<GuildInternalStubPtr>(nodeEntity, GuildInternal::NewStub(channel));

}

}// namespace guildpb
