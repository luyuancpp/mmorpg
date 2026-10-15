#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/data_service/data_service.grpc.pb.h"

#include "rpc/service_metadata/data_service_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace data_service {
using DataServiceStubPtr = std::unique_ptr<DataService::Stub>;
#pragma region DataServiceLoadPlayerData

struct AsyncDataServiceLoadPlayerDataGrpcClient {
    uint32_t messageId{ DataServiceLoadPlayerDataMessageId };
    ClientContext context;
    Status status;
    ::data_service::LoadPlayerDataResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::LoadPlayerDataResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::LoadPlayerDataRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceLoadPlayerDataHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::LoadPlayerDataResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceLoadPlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::LoadPlayerDataResponse&)>;
extern AsyncDataServiceLoadPlayerDataHandlerFunctionType AsyncDataServiceLoadPlayerDataHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::LoadPlayerDataRequest&)>;
extern AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType AsyncDataServiceLoadPlayerDataFailedHandler;

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request);
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, GrpcDone<::data_service::LoadPlayerDataResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceSavePlayerData

struct AsyncDataServiceSavePlayerDataGrpcClient {
    uint32_t messageId{ DataServiceSavePlayerDataMessageId };
    ClientContext context;
    Status status;
    ::data_service::SavePlayerDataResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::SavePlayerDataResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::SavePlayerDataRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceSavePlayerDataHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::SavePlayerDataResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceSavePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SavePlayerDataResponse&)>;
extern AsyncDataServiceSavePlayerDataHandlerFunctionType AsyncDataServiceSavePlayerDataHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceSavePlayerDataFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::SavePlayerDataRequest&)>;
extern AsyncDataServiceSavePlayerDataFailedHandlerFunctionType AsyncDataServiceSavePlayerDataFailedHandler;

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request);
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, GrpcDone<::data_service::SavePlayerDataResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerField

struct AsyncDataServiceGetPlayerFieldGrpcClient {
    uint32_t messageId{ DataServiceGetPlayerFieldMessageId };
    ClientContext context;
    Status status;
    ::data_service::GetPlayerFieldResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::GetPlayerFieldResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::GetPlayerFieldRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerFieldHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::GetPlayerFieldResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceGetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerFieldResponse&)>;
extern AsyncDataServiceGetPlayerFieldHandlerFunctionType AsyncDataServiceGetPlayerFieldHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerFieldFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerFieldRequest&)>;
extern AsyncDataServiceGetPlayerFieldFailedHandlerFunctionType AsyncDataServiceGetPlayerFieldFailedHandler;

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request);
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, GrpcDone<::data_service::GetPlayerFieldResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceSetPlayerField

struct AsyncDataServiceSetPlayerFieldGrpcClient {
    uint32_t messageId{ DataServiceSetPlayerFieldMessageId };
    ClientContext context;
    Status status;
    ::data_service::SetPlayerFieldResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::SetPlayerFieldResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::SetPlayerFieldRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceSetPlayerFieldHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::SetPlayerFieldResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceSetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SetPlayerFieldResponse&)>;
extern AsyncDataServiceSetPlayerFieldHandlerFunctionType AsyncDataServiceSetPlayerFieldHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceSetPlayerFieldFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::SetPlayerFieldRequest&)>;
extern AsyncDataServiceSetPlayerFieldFailedHandlerFunctionType AsyncDataServiceSetPlayerFieldFailedHandler;

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request);
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, GrpcDone<::data_service::SetPlayerFieldResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRegisterPlayerZone

struct AsyncDataServiceRegisterPlayerZoneGrpcClient {
    uint32_t messageId{ DataServiceRegisterPlayerZoneMessageId };
    ClientContext context;
    Status status;
    ::google::protobuf::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::google::protobuf::Empty>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::RegisterPlayerZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRegisterPlayerZoneHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::google::protobuf::Empty> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceRegisterPlayerZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::google::protobuf::Empty&)>;
extern AsyncDataServiceRegisterPlayerZoneHandlerFunctionType AsyncDataServiceRegisterPlayerZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRegisterPlayerZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RegisterPlayerZoneRequest&)>;
extern AsyncDataServiceRegisterPlayerZoneFailedHandlerFunctionType AsyncDataServiceRegisterPlayerZoneFailedHandler;

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request);
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone

struct AsyncDataServiceGetPlayerHomeZoneGrpcClient {
    uint32_t messageId{ DataServiceGetPlayerHomeZoneMessageId };
    ClientContext context;
    Status status;
    ::data_service::GetPlayerHomeZoneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::GetPlayerHomeZoneResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::GetPlayerHomeZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerHomeZoneHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::GetPlayerHomeZoneResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerHomeZoneResponse&)>;
extern AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerHomeZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerHomeZoneRequest&)>;
extern AsyncDataServiceGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneFailedHandler;

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request);
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, GrpcDone<::data_service::GetPlayerHomeZoneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone

struct AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient {
    uint32_t messageId{ DataServiceBatchGetPlayerHomeZoneMessageId };
    ClientContext context;
    Status status;
    ::data_service::BatchGetPlayerHomeZoneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::BatchGetPlayerHomeZoneResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::BatchGetPlayerHomeZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceBatchGetPlayerHomeZoneHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchGetPlayerHomeZoneResponse&)>;
extern AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceBatchGetPlayerHomeZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::BatchGetPlayerHomeZoneRequest&)>;
extern AsyncDataServiceBatchGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler;

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request);
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRemapHomeZoneForMerge

struct AsyncDataServiceRemapHomeZoneForMergeGrpcClient {
    uint32_t messageId{ DataServiceRemapHomeZoneForMergeMessageId };
    ClientContext context;
    Status status;
    ::data_service::RemapHomeZoneForMergeResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::RemapHomeZoneForMergeResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::RemapHomeZoneForMergeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRemapHomeZoneForMergeHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RemapHomeZoneForMergeResponse&)>;
extern AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRemapHomeZoneForMergeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RemapHomeZoneForMergeRequest&)>;
extern AsyncDataServiceRemapHomeZoneForMergeFailedHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeFailedHandler;

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request);
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceDeletePlayerData

struct AsyncDataServiceDeletePlayerDataGrpcClient {
    uint32_t messageId{ DataServiceDeletePlayerDataMessageId };
    ClientContext context;
    Status status;
    ::data_service::DeletePlayerDataResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::DeletePlayerDataResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::DeletePlayerDataRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceDeletePlayerDataHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::DeletePlayerDataResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceDeletePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::DeletePlayerDataResponse&)>;
extern AsyncDataServiceDeletePlayerDataHandlerFunctionType AsyncDataServiceDeletePlayerDataHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceDeletePlayerDataFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::DeletePlayerDataRequest&)>;
extern AsyncDataServiceDeletePlayerDataFailedHandlerFunctionType AsyncDataServiceDeletePlayerDataFailedHandler;

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request);
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, GrpcDone<::data_service::DeletePlayerDataResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceCreatePlayerSnapshot

struct AsyncDataServiceCreatePlayerSnapshotGrpcClient {
    uint32_t messageId{ DataServiceCreatePlayerSnapshotMessageId };
    ClientContext context;
    Status status;
    ::data_service::CreatePlayerSnapshotResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::CreatePlayerSnapshotResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::CreatePlayerSnapshotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceCreatePlayerSnapshotHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::CreatePlayerSnapshotResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::CreatePlayerSnapshotResponse&)>;
extern AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceCreatePlayerSnapshotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::CreatePlayerSnapshotRequest&)>;
extern AsyncDataServiceCreatePlayerSnapshotFailedHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotFailedHandler;

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request);
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, GrpcDone<::data_service::CreatePlayerSnapshotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceListPlayerSnapshots

struct AsyncDataServiceListPlayerSnapshotsGrpcClient {
    uint32_t messageId{ DataServiceListPlayerSnapshotsMessageId };
    ClientContext context;
    Status status;
    ::data_service::ListPlayerSnapshotsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::ListPlayerSnapshotsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::ListPlayerSnapshotsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceListPlayerSnapshotsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::ListPlayerSnapshotsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceListPlayerSnapshotsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::ListPlayerSnapshotsResponse&)>;
extern AsyncDataServiceListPlayerSnapshotsHandlerFunctionType AsyncDataServiceListPlayerSnapshotsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceListPlayerSnapshotsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::ListPlayerSnapshotsRequest&)>;
extern AsyncDataServiceListPlayerSnapshotsFailedHandlerFunctionType AsyncDataServiceListPlayerSnapshotsFailedHandler;

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request);
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, GrpcDone<::data_service::ListPlayerSnapshotsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerSnapshotDiff

struct AsyncDataServiceGetPlayerSnapshotDiffGrpcClient {
    uint32_t messageId{ DataServiceGetPlayerSnapshotDiffMessageId };
    ClientContext context;
    Status status;
    ::data_service::GetPlayerSnapshotDiffResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::GetPlayerSnapshotDiffResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::GetPlayerSnapshotDiffRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerSnapshotDiffHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerSnapshotDiffResponse&)>;
extern AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerSnapshotDiffFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerSnapshotDiffRequest&)>;
extern AsyncDataServiceGetPlayerSnapshotDiffFailedHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffFailedHandler;

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request);
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRollbackPlayer

struct AsyncDataServiceRollbackPlayerGrpcClient {
    uint32_t messageId{ DataServiceRollbackPlayerMessageId };
    ClientContext context;
    Status status;
    ::data_service::RollbackPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::RollbackPlayerResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::RollbackPlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRollbackPlayerHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::RollbackPlayerResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceRollbackPlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RollbackPlayerResponse&)>;
extern AsyncDataServiceRollbackPlayerHandlerFunctionType AsyncDataServiceRollbackPlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRollbackPlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RollbackPlayerRequest&)>;
extern AsyncDataServiceRollbackPlayerFailedHandlerFunctionType AsyncDataServiceRollbackPlayerFailedHandler;

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request);
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, GrpcDone<::data_service::RollbackPlayerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRollbackZone

struct AsyncDataServiceRollbackZoneGrpcClient {
    uint32_t messageId{ DataServiceRollbackZoneMessageId };
    ClientContext context;
    Status status;
    ::data_service::RollbackZoneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::RollbackZoneResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::RollbackZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRollbackZoneHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::RollbackZoneResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceRollbackZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RollbackZoneResponse&)>;
extern AsyncDataServiceRollbackZoneHandlerFunctionType AsyncDataServiceRollbackZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRollbackZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RollbackZoneRequest&)>;
extern AsyncDataServiceRollbackZoneFailedHandlerFunctionType AsyncDataServiceRollbackZoneFailedHandler;

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request);
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, GrpcDone<::data_service::RollbackZoneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRollbackAll

struct AsyncDataServiceRollbackAllGrpcClient {
    uint32_t messageId{ DataServiceRollbackAllMessageId };
    ClientContext context;
    Status status;
    ::data_service::RollbackAllResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::RollbackAllResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::RollbackAllRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRollbackAllHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::RollbackAllResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceRollbackAllHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RollbackAllResponse&)>;
extern AsyncDataServiceRollbackAllHandlerFunctionType AsyncDataServiceRollbackAllHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRollbackAllFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RollbackAllRequest&)>;
extern AsyncDataServiceRollbackAllFailedHandlerFunctionType AsyncDataServiceRollbackAllFailedHandler;

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request);
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, GrpcDone<::data_service::RollbackAllResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceBatchRecallItems

struct AsyncDataServiceBatchRecallItemsGrpcClient {
    uint32_t messageId{ DataServiceBatchRecallItemsMessageId };
    ClientContext context;
    Status status;
    ::data_service::BatchRecallItemsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::BatchRecallItemsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::BatchRecallItemsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceBatchRecallItemsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::BatchRecallItemsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceBatchRecallItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchRecallItemsResponse&)>;
extern AsyncDataServiceBatchRecallItemsHandlerFunctionType AsyncDataServiceBatchRecallItemsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceBatchRecallItemsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::BatchRecallItemsRequest&)>;
extern AsyncDataServiceBatchRecallItemsFailedHandlerFunctionType AsyncDataServiceBatchRecallItemsFailedHandler;

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request);
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, GrpcDone<::data_service::BatchRecallItemsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceQueryTransactionLog

struct AsyncDataServiceQueryTransactionLogGrpcClient {
    uint32_t messageId{ DataServiceQueryTransactionLogMessageId };
    ClientContext context;
    Status status;
    ::data_service::QueryTransactionLogResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::QueryTransactionLogResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::QueryTransactionLogRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceQueryTransactionLogHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::QueryTransactionLogResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceQueryTransactionLogHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::QueryTransactionLogResponse&)>;
extern AsyncDataServiceQueryTransactionLogHandlerFunctionType AsyncDataServiceQueryTransactionLogHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceQueryTransactionLogFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::QueryTransactionLogRequest&)>;
extern AsyncDataServiceQueryTransactionLogFailedHandlerFunctionType AsyncDataServiceQueryTransactionLogFailedHandler;

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request);
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, GrpcDone<::data_service::QueryTransactionLogResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceCreateEventSnapshot

struct AsyncDataServiceCreateEventSnapshotGrpcClient {
    uint32_t messageId{ DataServiceCreateEventSnapshotMessageId };
    ClientContext context;
    Status status;
    ::data_service::CreateEventSnapshotResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::CreateEventSnapshotResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::CreateEventSnapshotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceCreateEventSnapshotHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::CreateEventSnapshotResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceCreateEventSnapshotHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::CreateEventSnapshotResponse&)>;
extern AsyncDataServiceCreateEventSnapshotHandlerFunctionType AsyncDataServiceCreateEventSnapshotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceCreateEventSnapshotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::CreateEventSnapshotRequest&)>;
extern AsyncDataServiceCreateEventSnapshotFailedHandlerFunctionType AsyncDataServiceCreateEventSnapshotFailedHandler;

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request);
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, GrpcDone<::data_service::CreateEventSnapshotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceAllocateIdSegment

struct AsyncDataServiceAllocateIdSegmentGrpcClient {
    uint32_t messageId{ DataServiceAllocateIdSegmentMessageId };
    ClientContext context;
    Status status;
    ::data_service::AllocateIdSegmentResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::AllocateIdSegmentResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::AllocateIdSegmentRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceAllocateIdSegmentHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::AllocateIdSegmentResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceAllocateIdSegmentHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::AllocateIdSegmentResponse&)>;
extern AsyncDataServiceAllocateIdSegmentHandlerFunctionType AsyncDataServiceAllocateIdSegmentHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceAllocateIdSegmentFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::AllocateIdSegmentRequest&)>;
extern AsyncDataServiceAllocateIdSegmentFailedHandlerFunctionType AsyncDataServiceAllocateIdSegmentFailedHandler;

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request);
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, GrpcDone<::data_service::AllocateIdSegmentResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceReservePlayerName

struct AsyncDataServiceReservePlayerNameGrpcClient {
    uint32_t messageId{ DataServiceReservePlayerNameMessageId };
    ClientContext context;
    Status status;
    ::data_service::ReservePlayerNameResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::ReservePlayerNameResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::ReservePlayerNameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceReservePlayerNameHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::ReservePlayerNameResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceReservePlayerNameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::ReservePlayerNameResponse&)>;
extern AsyncDataServiceReservePlayerNameHandlerFunctionType AsyncDataServiceReservePlayerNameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceReservePlayerNameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::ReservePlayerNameRequest&)>;
extern AsyncDataServiceReservePlayerNameFailedHandlerFunctionType AsyncDataServiceReservePlayerNameFailedHandler;

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request);
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, GrpcDone<::data_service::ReservePlayerNameResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceReleasePlayerName

struct AsyncDataServiceReleasePlayerNameGrpcClient {
    uint32_t messageId{ DataServiceReleasePlayerNameMessageId };
    ClientContext context;
    Status status;
    ::google::protobuf::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::google::protobuf::Empty>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::ReleasePlayerNameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceReleasePlayerNameHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::google::protobuf::Empty> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceReleasePlayerNameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::google::protobuf::Empty&)>;
extern AsyncDataServiceReleasePlayerNameHandlerFunctionType AsyncDataServiceReleasePlayerNameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceReleasePlayerNameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::ReleasePlayerNameRequest&)>;
extern AsyncDataServiceReleasePlayerNameFailedHandlerFunctionType AsyncDataServiceReleasePlayerNameFailedHandler;

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request);
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceBatchGetPlayerName

struct AsyncDataServiceBatchGetPlayerNameGrpcClient {
    uint32_t messageId{ DataServiceBatchGetPlayerNameMessageId };
    ClientContext context;
    Status status;
    ::data_service::BatchGetPlayerNameResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::BatchGetPlayerNameResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::BatchGetPlayerNameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceBatchGetPlayerNameHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::BatchGetPlayerNameResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceBatchGetPlayerNameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchGetPlayerNameResponse&)>;
extern AsyncDataServiceBatchGetPlayerNameHandlerFunctionType AsyncDataServiceBatchGetPlayerNameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceBatchGetPlayerNameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::BatchGetPlayerNameRequest&)>;
extern AsyncDataServiceBatchGetPlayerNameFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerNameFailedHandler;

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request);
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, GrpcDone<::data_service::BatchGetPlayerNameResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerAssetOpLedger

struct AsyncDataServiceGetPlayerAssetOpLedgerGrpcClient {
    uint32_t messageId{ DataServiceGetPlayerAssetOpLedgerMessageId };
    ClientContext context;
    Status status;
    ::data_service::GetPlayerAssetOpLedgerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::data_service::GetPlayerAssetOpLedgerResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::data_service::GetPlayerAssetOpLedgerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerAssetOpLedgerHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerAssetOpLedgerResponse&)>;
extern AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerAssetOpLedgerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerAssetOpLedgerRequest&)>;
extern AsyncDataServiceGetPlayerAssetOpLedgerFailedHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler;

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request);
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetDataServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDataServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDataServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetDataServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetDataServiceCallDeadline(std::chrono::milliseconds deadline);
void HandleDataServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitDataServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace data_service
