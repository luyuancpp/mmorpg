#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "core/network/grpc_client_call.h"

#include "rpc/service_metadata/mail_admin_service_metadata.h"

namespace mailpb {
#pragma region MailAdminSendPersonalMail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMailAdminSendPersonalMailGrpcPath = "/mailpb.MailAdmin/SendPersonalMail";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendMailAdminSendPersonalMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request, GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendMailAdminSendPersonalMailErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region MailAdminSendSystemMail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMailAdminSendSystemMailGrpcPath = "/mailpb.MailAdmin/SendSystemMail";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendMailAdminSendSystemMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request, GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendMailAdminSendSystemMailErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region MailAdminSendGuildMail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMailAdminSendGuildMailGrpcPath = "/mailpb.MailAdmin/SendGuildMail";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendMailAdminSendGuildMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request, GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendMailAdminSendGuildMailErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region MailAdminImportMailRecipients
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMailAdminImportMailRecipientsGrpcPath = "/mailpb.MailAdmin/ImportMailRecipients";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendMailAdminImportMailRecipients(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ImportMailRecipientsRequest& request, GrpcDone<::mailpb::ImportMailRecipientsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendMailAdminImportMailRecipientsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region MailAdminSealMailCampaign
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMailAdminSealMailCampaignGrpcPath = "/mailpb.MailAdmin/SealMailCampaign";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendMailAdminSealMailCampaign(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SealMailCampaignRequest& request, GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendMailAdminSealMailCampaignErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetMailAdminCallDeadline(std::chrono::milliseconds deadline);

}// namespace mailpb
