#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "core/network/grpc_client_call.h"

#include "rpc/service_metadata/etcd_service_metadata.h"

namespace etcdserverpb {
#pragma region KVRange
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVRangeGrpcPath = "/etcdserverpb.KV/Range";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, GrpcDone<::etcdserverpb::RangeResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendKVRangeErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region KVPut
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVPutGrpcPath = "/etcdserverpb.KV/Put";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, GrpcDone<::etcdserverpb::PutResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendKVPutErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region KVDeleteRange
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVDeleteRangeGrpcPath = "/etcdserverpb.KV/DeleteRange";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, GrpcDone<::etcdserverpb::DeleteRangeResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendKVDeleteRangeErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region KVTxn
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVTxnGrpcPath = "/etcdserverpb.KV/Txn";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, GrpcDone<::etcdserverpb::TxnResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendKVTxnErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region KVCompact
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kKVCompactGrpcPath = "/etcdserverpb.KV/Compact";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, GrpcDone<::etcdserverpb::CompactionResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendKVCompactErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region WatchWatch
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kWatchWatchGrpcPath = "/etcdserverpb.Watch/Watch";
// 流式方法不生成 Send,也不进入元数据 sender;etcd 流由 MuduoEtcdTransport 手写。
#pragma endregion
#pragma region LeaseLeaseGrant
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseGrantGrpcPath = "/etcdserverpb.Lease/LeaseGrant";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, GrpcDone<::etcdserverpb::LeaseGrantResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendLeaseLeaseGrantErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region LeaseLeaseRevoke
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseRevokeGrpcPath = "/etcdserverpb.Lease/LeaseRevoke";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, GrpcDone<::etcdserverpb::LeaseRevokeResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendLeaseLeaseRevokeErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region LeaseLeaseKeepAlive
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseKeepAliveGrpcPath = "/etcdserverpb.Lease/LeaseKeepAlive";
// 流式方法不生成 Send,也不进入元数据 sender;etcd 流由 MuduoEtcdTransport 手写。
#pragma endregion
#pragma region LeaseLeaseTimeToLive
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseTimeToLiveGrpcPath = "/etcdserverpb.Lease/LeaseTimeToLive";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendLeaseLeaseTimeToLiveErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region LeaseLeaseLeases
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLeaseLeaseLeasesGrpcPath = "/etcdserverpb.Lease/LeaseLeases";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, GrpcDone<::etcdserverpb::LeaseLeasesResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendLeaseLeaseLeasesErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetEtcdCallDeadline(std::chrono::milliseconds deadline);

}// namespace etcdserverpb
