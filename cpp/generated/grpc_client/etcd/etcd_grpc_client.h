#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/etcd_service_metadata.h"

namespace etcdserverpb {
#pragma region KVRange
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVRangeGrpcPath = "/etcdserverpb.KV/Range";
using AsyncKVRangeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::RangeResponse&)>;
extern AsyncKVRangeHandlerFunctionType AsyncKVRangeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncKVRangeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::RangeRequest&)>;
extern AsyncKVRangeFailedHandlerFunctionType AsyncKVRangeFailedHandler;

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request);
void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, GrpcDone<::etcdserverpb::RangeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region KVPut
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVPutGrpcPath = "/etcdserverpb.KV/Put";
using AsyncKVPutHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::PutResponse&)>;
extern AsyncKVPutHandlerFunctionType AsyncKVPutHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncKVPutFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::PutRequest&)>;
extern AsyncKVPutFailedHandlerFunctionType AsyncKVPutFailedHandler;

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request);
void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, GrpcDone<::etcdserverpb::PutResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region KVDeleteRange
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVDeleteRangeGrpcPath = "/etcdserverpb.KV/DeleteRange";
using AsyncKVDeleteRangeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::DeleteRangeResponse&)>;
extern AsyncKVDeleteRangeHandlerFunctionType AsyncKVDeleteRangeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncKVDeleteRangeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::DeleteRangeRequest&)>;
extern AsyncKVDeleteRangeFailedHandlerFunctionType AsyncKVDeleteRangeFailedHandler;

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request);
void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, GrpcDone<::etcdserverpb::DeleteRangeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region KVTxn
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVTxnGrpcPath = "/etcdserverpb.KV/Txn";
using AsyncKVTxnHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::TxnResponse&)>;
extern AsyncKVTxnHandlerFunctionType AsyncKVTxnHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncKVTxnFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::TxnRequest&)>;
extern AsyncKVTxnFailedHandlerFunctionType AsyncKVTxnFailedHandler;

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request);
void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, GrpcDone<::etcdserverpb::TxnResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region KVCompact
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVCompactGrpcPath = "/etcdserverpb.KV/Compact";
using AsyncKVCompactHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::CompactionResponse&)>;
extern AsyncKVCompactHandlerFunctionType AsyncKVCompactHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncKVCompactFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::CompactionRequest&)>;
extern AsyncKVCompactFailedHandlerFunctionType AsyncKVCompactFailedHandler;

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request);
void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, GrpcDone<::etcdserverpb::CompactionResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region WatchWatch
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kWatchWatchGrpcPath = "/etcdserverpb.Watch/Watch";
// 流式方法不生成客户端:全仓唯一的流式服务 etcd 由 MuduoEtcdTransport 手写。下面三个重载只为 gRpcMethodRegistry
// 的通用 sender 保留签名,调用即记 ERROR 拒绝。

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request);
void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region LeaseLeaseGrant
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseGrantGrpcPath = "/etcdserverpb.Lease/LeaseGrant";
using AsyncLeaseLeaseGrantHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::LeaseGrantResponse&)>;
extern AsyncLeaseLeaseGrantHandlerFunctionType AsyncLeaseLeaseGrantHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLeaseLeaseGrantFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::LeaseGrantRequest&)>;
extern AsyncLeaseLeaseGrantFailedHandlerFunctionType AsyncLeaseLeaseGrantFailedHandler;

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request);
void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, GrpcDone<::etcdserverpb::LeaseGrantResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region LeaseLeaseRevoke
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseRevokeGrpcPath = "/etcdserverpb.Lease/LeaseRevoke";
using AsyncLeaseLeaseRevokeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::LeaseRevokeResponse&)>;
extern AsyncLeaseLeaseRevokeHandlerFunctionType AsyncLeaseLeaseRevokeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLeaseLeaseRevokeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::LeaseRevokeRequest&)>;
extern AsyncLeaseLeaseRevokeFailedHandlerFunctionType AsyncLeaseLeaseRevokeFailedHandler;

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request);
void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, GrpcDone<::etcdserverpb::LeaseRevokeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region LeaseLeaseKeepAlive
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseKeepAliveGrpcPath = "/etcdserverpb.Lease/LeaseKeepAlive";
// 流式方法不生成客户端:全仓唯一的流式服务 etcd 由 MuduoEtcdTransport 手写。下面三个重载只为 gRpcMethodRegistry
// 的通用 sender 保留签名,调用即记 ERROR 拒绝。

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request);
void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region LeaseLeaseTimeToLive
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseTimeToLiveGrpcPath = "/etcdserverpb.Lease/LeaseTimeToLive";
using AsyncLeaseLeaseTimeToLiveHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::LeaseTimeToLiveResponse&)>;
extern AsyncLeaseLeaseTimeToLiveHandlerFunctionType AsyncLeaseLeaseTimeToLiveHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLeaseLeaseTimeToLiveFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::LeaseTimeToLiveRequest&)>;
extern AsyncLeaseLeaseTimeToLiveFailedHandlerFunctionType AsyncLeaseLeaseTimeToLiveFailedHandler;

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request);
void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region LeaseLeaseLeases
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseLeasesGrpcPath = "/etcdserverpb.Lease/LeaseLeases";
using AsyncLeaseLeaseLeasesHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::etcdserverpb::LeaseLeasesResponse&)>;
extern AsyncLeaseLeaseLeasesHandlerFunctionType AsyncLeaseLeaseLeasesHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLeaseLeaseLeasesFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::etcdserverpb::LeaseLeasesRequest&)>;
extern AsyncLeaseLeaseLeasesFailedHandlerFunctionType AsyncLeaseLeaseLeasesFailedHandler;

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request);
void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, GrpcDone<::etcdserverpb::LeaseLeasesResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetEtcdHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetEtcdIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetEtcdFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetEtcdIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetEtcdCallDeadline(std::chrono::milliseconds deadline);

}// namespace etcdserverpb
