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

void AsyncCompleteGrpcGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient*>(got_tag));
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

	GuildInternalListAppliedAssetOpsSincePool.destroy(call);
}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request) {

    SendGuildInternalListAppliedAssetOpsSince(registry, nodeEntity, request, {}, {});

}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(GuildInternalListAppliedAssetOpsSincePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    call->sentMetadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
        call->sentMetadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    call->request = request;
    call->context.set_deadline(NextCallDeadline());

    call->response_reader = registry
        .get<GuildInternalStubPtr>(nodeEntity)
        ->PrepareAsyncListAppliedAssetOpsSince(&call->context, call->request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildInternalListAppliedAssetOpsSinceMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ListAppliedAssetOpsSinceRequest& derived = static_cast<const ::guildpb::ListAppliedAssetOpsSinceRequest&>(message);
    SendGuildInternalListAppliedAssetOpsSince(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleGuildInternalCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
        switch (grpcTag->messageId) {
        case GuildInternalListAppliedAssetOpsSinceMessageId:
            AsyncCompleteGrpcGuildInternalListAppliedAssetOpsSince(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
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
