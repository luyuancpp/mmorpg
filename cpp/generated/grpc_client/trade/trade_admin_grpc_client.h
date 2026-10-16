#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/trade/trade_admin.grpc.pb.h"

#include "rpc/service_metadata/trade_admin_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace trade {
using TradeAdminStubPtr = std::unique_ptr<TradeAdmin::Stub>;
#pragma region TradeAdminSeedListing

struct AsyncTradeAdminSeedListingGrpcClient {
    uint32_t messageId{ TradeAdminSeedListingMessageId };
    ClientContext context;
    Status status;
    ::trade::SeedListingResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::trade::SeedListingResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::trade::SeedListingRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncTradeAdminSeedListingHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::trade::SeedListingResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncTradeAdminSeedListingHandlerFunctionType =
    std::function<void(const ClientContext&, const ::trade::SeedListingResponse&)>;
extern AsyncTradeAdminSeedListingHandlerFunctionType AsyncTradeAdminSeedListingHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncTradeAdminSeedListingFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::SeedListingRequest&)>;
extern AsyncTradeAdminSeedListingFailedHandlerFunctionType AsyncTradeAdminSeedListingFailedHandler;

void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request);
void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendTradeAdminSeedListing(entt::registry& registry, entt::entity nodeEntity, const ::trade::SeedListingRequest& request, GrpcDone<::trade::SeedListingResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetTradeAdminHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetTradeAdminIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetTradeAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetTradeAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetTradeAdminCallDeadline(std::chrono::milliseconds deadline);
void HandleTradeAdminCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitTradeAdminGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace trade
