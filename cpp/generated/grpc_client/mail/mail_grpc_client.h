#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/mail_service_metadata.h"

namespace mailpb {
#pragma region ClientPlayerMailListMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailListMailsGrpcPath = "/mailpb.ClientPlayerMail/ListMails";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request, GrpcDone<::mailpb::ListMailsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerMailListMailsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerMailReadMail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailReadMailGrpcPath = "/mailpb.ClientPlayerMail/ReadMail";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request, GrpcDone<::mailpb::ReadMailResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerMailReadMailErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerMailClaimMailAttachments
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailClaimMailAttachmentsGrpcPath = "/mailpb.ClientPlayerMail/ClaimMailAttachments";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request, GrpcDone<::mailpb::ClaimMailAttachmentsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerMailClaimMailAttachmentsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerMailDeleteMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailDeleteMailsGrpcPath = "/mailpb.ClientPlayerMail/DeleteMails";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request, GrpcDone<::mailpb::DeleteMailsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerMailDeleteMailsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerMailClearReadMails
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailClearReadMailsGrpcPath = "/mailpb.ClientPlayerMail/ClearReadMails";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request, GrpcDone<::mailpb::ClearReadMailsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerMailClearReadMailsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerMailNotifyMailChanged
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerMailNotifyMailChangedGrpcPath = "/mailpb.ClientPlayerMail/NotifyMailChanged";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request, GrpcDone<::mailpb::MailChangedEvent> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerMailNotifyMailChangedErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetMailCallDeadline(std::chrono::milliseconds deadline);

}// namespace mailpb
