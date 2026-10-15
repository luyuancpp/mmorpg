#include "muduo/base/Logging.h"

#include "match_internal_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetMatchInternalCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace match {
struct MatchInternalCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region MatchInternalStartActivityBattle
boost::object_pool<AsyncMatchInternalStartActivityBattleGrpcClient> MatchInternalStartActivityBattlePool;
using AsyncMatchInternalStartActivityBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::StartActivityBattleResponse&)>;
AsyncMatchInternalStartActivityBattleHandlerFunctionType AsyncMatchInternalStartActivityBattleHandler;
AsyncMatchInternalStartActivityBattleFailedHandlerFunctionType AsyncMatchInternalStartActivityBattleFailedHandler;

void CompleteMatchInternalStartActivityBattle(AsyncMatchInternalStartActivityBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchInternalStartActivityBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchInternalStartActivityBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchInternalStartActivityBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchInternalStartActivityBattleHandler) {
            AsyncMatchInternalStartActivityBattleHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchInternal.StartActivityBattle reply dropped: AsyncMatchInternalStartActivityBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchInternalStartActivityBattleFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchInternal.StartActivityBattle", call->context, call->status, call->sentMetadata};
        AsyncMatchInternalStartActivityBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchInternal.StartActivityBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchInternalStartActivityBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchInternalStartActivityBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchInternalStartActivityBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchInternalStartActivityBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::match::StartActivityBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::StartActivityBattleResponse> done) {
    auto call(MatchInternalStartActivityBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchInternalStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchInternalStartActivityBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncStartActivityBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchInternalStartActivityBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchInternalStartActivityBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::StartActivityBattleRequest& request,
                                            GrpcDone<::match::StartActivityBattleResponse> done, const GrpcCallOptions& options) {
    StartMatchInternalStartActivityBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchInternalStartActivityBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::StartActivityBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchInternalStartActivityBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchInternalStartActivityBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::StartActivityBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchInternalStartActivityBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchInternalStartActivityBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::StartActivityBattleRequest& derived = static_cast<const ::match::StartActivityBattleRequest&>(message);
    SendMatchInternalStartActivityBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleMatchInternalCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case MatchInternalStartActivityBattleMessageId:
            AsyncCompleteGrpcMatchInternalStartActivityBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetMatchInternalHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncMatchInternalStartActivityBattleHandler = handler;
}

void SetMatchInternalIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncMatchInternalStartActivityBattleHandler) {
        AsyncMatchInternalStartActivityBattleHandler = handler;
    }
}

void SetMatchInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncMatchInternalStartActivityBattleFailedHandler = handler;
}

void SetMatchInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncMatchInternalStartActivityBattleFailedHandler) {
        AsyncMatchInternalStartActivityBattleFailedHandler = handler;
    }
}

void SetMatchInternalCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitMatchInternalGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<MatchInternalStubPtr>(nodeEntity, MatchInternal::NewStub(channel));

}

}// namespace match
