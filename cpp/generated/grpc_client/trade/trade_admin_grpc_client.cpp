#include "muduo/base/Logging.h"

#include "trade_admin_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetTradeAdminCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace trade {
struct TradeAdminCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region TradeAdminSeedListing
boost::object_pool<AsyncTradeAdminSeedListingGrpcClient> TradeAdminSeedListingPool;
using AsyncTradeAdminSeedListingHandlerFunctionType =
    std::function<void(const ClientContext&, const ::trade::SeedListingResponse&)>;
AsyncTradeAdminSeedListingHandlerFunctionType AsyncTradeAdminSeedListingHandler;
AsyncTradeAdminSeedListingFailedHandlerFunctionType AsyncTradeAdminSeedListingFailedHandler;

void CompleteTradeAdminSeedListing(AsyncTradeAdminSeedListingGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncTradeAdminSeedListingGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        TradeAdminSeedListingPool.destroy(completed);
    };
    const std::unique_ptr<AsyncTradeAdminSeedListingGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncTradeAdminSeedListingHandler) {
            AsyncTradeAdminSeedListingHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC TradeAdmin.SeedListing reply dropped: AsyncTradeAdminSeedListingHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncTradeAdminSeedListingFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "TradeAdmin.SeedListing", call->context, call->status, call->sentMetadata};
        AsyncTradeAdminSeedListingFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC TradeAdmin.SeedListing failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcTradeAdminSeedListing(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncTradeAdminSeedListingGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteTradeAdminSeedListing(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartTradeAdminSeedListingCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::SeedListingResponse> done) {
    auto call(TradeAdminSeedListingPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<TradeAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteTradeAdminSeedListing(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSeedListing(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(TradeAdminSeedListingMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request,
                                            GrpcDone<::trade::SeedListingResponse> done, const GrpcCallOptions& options) {
    StartTradeAdminSeedListingCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendTradeAdminSeedListing(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartTradeAdminSeedListingCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::trade::SeedListingRequest& derived = static_cast<const ::trade::SeedListingRequest&>(message);
    SendTradeAdminSeedListing(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleTradeAdminCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case TradeAdminSeedListingMessageId:
            AsyncCompleteGrpcTradeAdminSeedListing(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetTradeAdminHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncTradeAdminSeedListingHandler = handler;
}

void SetTradeAdminIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncTradeAdminSeedListingHandler) {
        AsyncTradeAdminSeedListingHandler = handler;
    }
}

void SetTradeAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncTradeAdminSeedListingFailedHandler = handler;
}

void SetTradeAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncTradeAdminSeedListingFailedHandler) {
        AsyncTradeAdminSeedListingFailedHandler = handler;
    }
}

void SetTradeAdminCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitTradeAdminGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<TradeAdminStubPtr>(nodeEntity, TradeAdmin::NewStub(channel));

}

}// namespace trade
