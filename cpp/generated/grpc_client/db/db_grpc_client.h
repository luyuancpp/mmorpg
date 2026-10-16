#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/db_service_metadata.h"

#pragma region dbTest
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kdbTestGrpcPath = "/db/Test";
using AsyncdbTestHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::TestResponse&)>;
extern AsyncdbTestHandlerFunctionType AsyncdbTestHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncdbTestFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::TestRequest&)>;
extern AsyncdbTestFailedHandlerFunctionType AsyncdbTestFailedHandler;

void SenddbTest(entt::registry& registry, entt::entity nodeEntity, const ::TestRequest& request);
void SenddbTest(entt::registry& registry, entt::entity nodeEntity, const ::TestRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SenddbTest(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SenddbTest(entt::registry& registry, entt::entity nodeEntity, const ::TestRequest& request, GrpcDone<::TestResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetDbHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDbIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDbFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetDbIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetDbCallDeadline(std::chrono::milliseconds deadline);
