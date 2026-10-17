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
#pragma region ClientPlayerMailClearReadMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailClearReadMailsGrpcPath = "/mailpb.ClientPlayerMail/ClearReadMails";
using AsyncClientPlayerMailClearReadMailsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::ClearReadMailsResponse&)>;
extern AsyncClientPlayerMailClearReadMailsHandlerFunctionType AsyncClientPlayerMailClearReadMailsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailClearReadMailsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::ClearReadMailsRequest&)>;
extern AsyncClientPlayerMailClearReadMailsFailedHandlerFunctionType AsyncClientPlayerMailClearReadMailsFailedHandler;

void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request);
void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request, GrpcDone<::mailpb::ClearReadMailsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerMailNotifyMailChanged
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailNotifyMailChangedGrpcPath = "/mailpb.ClientPlayerMail/NotifyMailChanged";
using AsyncClientPlayerMailNotifyMailChangedHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::mailpb::MailChangedEvent&)>;
extern AsyncClientPlayerMailNotifyMailChangedHandlerFunctionType AsyncClientPlayerMailNotifyMailChangedHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerMailNotifyMailChangedFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::mailpb::MailChangedEvent&)>;
extern AsyncClientPlayerMailNotifyMailChangedFailedHandlerFunctionType AsyncClientPlayerMailNotifyMailChangedFailedHandler;

void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request);
void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request, GrpcDone<::mailpb::MailChangedEvent> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetMailHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetMailIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetMailFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetMailIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetMailCallDeadline(std::chrono::milliseconds deadline);

}// namespace mailpb
