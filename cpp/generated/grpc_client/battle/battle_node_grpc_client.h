#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/battle_node_service_metadata.h"

#pragma region BattleNodeQueryBattleCreation
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kBattleNodeQueryBattleCreationGrpcPath = "/BattleNode/QueryBattleCreation";
using AsyncBattleNodeQueryBattleCreationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::QueryBattleCreationResponse&)>;
extern AsyncBattleNodeQueryBattleCreationHandlerFunctionType AsyncBattleNodeQueryBattleCreationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncBattleNodeQueryBattleCreationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::QueryBattleCreationRequest&)>;
extern AsyncBattleNodeQueryBattleCreationFailedHandlerFunctionType AsyncBattleNodeQueryBattleCreationFailedHandler;

void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request);
void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request, GrpcDone<::QueryBattleCreationResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region BattleNodeCreateBattle
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kBattleNodeCreateBattleGrpcPath = "/BattleNode/CreateBattle";
using AsyncBattleNodeCreateBattleHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::CreateBattleResponse&)>;
extern AsyncBattleNodeCreateBattleHandlerFunctionType AsyncBattleNodeCreateBattleHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncBattleNodeCreateBattleFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::CreateBattleRequest&)>;
extern AsyncBattleNodeCreateBattleFailedHandlerFunctionType AsyncBattleNodeCreateBattleFailedHandler;

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request);
void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request, GrpcDone<::CreateBattleResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region BattleNodeDestroyBattle
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kBattleNodeDestroyBattleGrpcPath = "/BattleNode/DestroyBattle";
using AsyncBattleNodeDestroyBattleHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncBattleNodeDestroyBattleHandlerFunctionType AsyncBattleNodeDestroyBattleHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncBattleNodeDestroyBattleFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::DestroyBattleRequest&)>;
extern AsyncBattleNodeDestroyBattleFailedHandlerFunctionType AsyncBattleNodeDestroyBattleFailedHandler;

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request);
void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region BattleNodeAddObserver
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kBattleNodeAddObserverGrpcPath = "/BattleNode/AddObserver";
using AsyncBattleNodeAddObserverHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::AddObserverResponse&)>;
extern AsyncBattleNodeAddObserverHandlerFunctionType AsyncBattleNodeAddObserverHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncBattleNodeAddObserverFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::AddObserverRequest&)>;
extern AsyncBattleNodeAddObserverFailedHandlerFunctionType AsyncBattleNodeAddObserverFailedHandler;

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request);
void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request, GrpcDone<::AddObserverResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region BattleNodeRemoveObserver
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kBattleNodeRemoveObserverGrpcPath = "/BattleNode/RemoveObserver";
using AsyncBattleNodeRemoveObserverHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncBattleNodeRemoveObserverHandlerFunctionType AsyncBattleNodeRemoveObserverHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncBattleNodeRemoveObserverFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::RemoveObserverRequest&)>;
extern AsyncBattleNodeRemoveObserverFailedHandlerFunctionType AsyncBattleNodeRemoveObserverFailedHandler;

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request);
void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region BattleNodeIssueBattleTicket
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kBattleNodeIssueBattleTicketGrpcPath = "/BattleNode/IssueBattleTicket";
using AsyncBattleNodeIssueBattleTicketHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::IssueBattleTicketResponse&)>;
extern AsyncBattleNodeIssueBattleTicketHandlerFunctionType AsyncBattleNodeIssueBattleTicketHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncBattleNodeIssueBattleTicketFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::IssueBattleTicketRequest&)>;
extern AsyncBattleNodeIssueBattleTicketFailedHandlerFunctionType AsyncBattleNodeIssueBattleTicketFailedHandler;

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request);
void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request, GrpcDone<::IssueBattleTicketResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetBattleNodeHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetBattleNodeIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetBattleNodeFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetBattleNodeIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetBattleNodeCallDeadline(std::chrono::milliseconds deadline);
