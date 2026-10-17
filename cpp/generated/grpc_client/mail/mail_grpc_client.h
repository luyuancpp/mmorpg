#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/mail_service_metadata.h"

namespace mailpb {
#pragma region ClientPlayerMailListMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailListMailsGrpcPath = "/mailpb.ClientPlayerMail/ListMails";
using AsyncClientPlayerMailListMailsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::ListMailsResponse&)>;
extern AsyncClientPlayerMailListMailsHandlerFunctionType AsyncClientPlayerMailListMailsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailListMailsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::ListMailsRequest&)>;
extern AsyncClientPlayerMailListMailsFailedHandlerFunctionType AsyncClientPlayerMailListMailsFailedHandler;

void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request);
void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request, GrpcDone<::mailpb::ListMailsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerMailReadMail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailReadMailGrpcPath = "/mailpb.ClientPlayerMail/ReadMail";
using AsyncClientPlayerMailReadMailHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::ReadMailResponse&)>;
extern AsyncClientPlayerMailReadMailHandlerFunctionType AsyncClientPlayerMailReadMailHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailReadMailFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::ReadMailRequest&)>;
extern AsyncClientPlayerMailReadMailFailedHandlerFunctionType AsyncClientPlayerMailReadMailFailedHandler;

void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request);
void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request, GrpcDone<::mailpb::ReadMailResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerMailDeleteMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailDeleteMailsGrpcPath = "/mailpb.ClientPlayerMail/DeleteMails";
using AsyncClientPlayerMailDeleteMailsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::DeleteMailsResponse&)>;
extern AsyncClientPlayerMailDeleteMailsHandlerFunctionType AsyncClientPlayerMailDeleteMailsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailDeleteMailsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::DeleteMailsRequest&)>;
extern AsyncClientPlayerMailDeleteMailsFailedHandlerFunctionType AsyncClientPlayerMailDeleteMailsFailedHandler;

void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request);
void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request, GrpcDone<::mailpb::DeleteMailsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerMailDeleteReadMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailDeleteReadMailsGrpcPath = "/mailpb.ClientPlayerMail/DeleteReadMails";
using AsyncClientPlayerMailDeleteReadMailsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::DeleteReadMailsResponse&)>;
extern AsyncClientPlayerMailDeleteReadMailsHandlerFunctionType AsyncClientPlayerMailDeleteReadMailsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailDeleteReadMailsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::DeleteReadMailsRequest&)>;
extern AsyncClientPlayerMailDeleteReadMailsFailedHandlerFunctionType AsyncClientPlayerMailDeleteReadMailsFailedHandler;

void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request);
void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request, GrpcDone<::mailpb::DeleteReadMailsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerMailClaimMailAttachments
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailClaimMailAttachmentsGrpcPath = "/mailpb.ClientPlayerMail/ClaimMailAttachments";
using AsyncClientPlayerMailClaimMailAttachmentsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::ClaimMailAttachmentsResponse&)>;
extern AsyncClientPlayerMailClaimMailAttachmentsHandlerFunctionType AsyncClientPlayerMailClaimMailAttachmentsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailClaimMailAttachmentsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::ClaimMailAttachmentsRequest&)>;
extern AsyncClientPlayerMailClaimMailAttachmentsFailedHandlerFunctionType AsyncClientPlayerMailClaimMailAttachmentsFailedHandler;

void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request);
void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request, GrpcDone<::mailpb::ClaimMailAttachmentsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerMailNotifyMailEvent
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailNotifyMailEventGrpcPath = "/mailpb.ClientPlayerMail/NotifyMailEvent";
using AsyncClientPlayerMailNotifyMailEventHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncClientPlayerMailNotifyMailEventHandlerFunctionType AsyncClientPlayerMailNotifyMailEventHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailNotifyMailEventFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::MailEventS2C&)>;
extern AsyncClientPlayerMailNotifyMailEventFailedHandlerFunctionType AsyncClientPlayerMailNotifyMailEventFailedHandler;

void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request);
void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetMailHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetMailIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetMailFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetMailIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetMailCallDeadline(std::chrono::milliseconds deadline);

}// namespace mailpb
