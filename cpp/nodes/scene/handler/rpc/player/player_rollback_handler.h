#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/player_rollback.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class SceneRollbackClientPlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GmAttachDebt(entt::entity player,
        const ::GmAttachDebtRequest* request,
        ::GmAttachDebtResponse* response);
    static void GmWaiveDebt(entt::entity player,
        const ::GmWaiveDebtRequest* request,
        ::GmWaiveDebtResponse* response);
    static void GmAdjustDebt(entt::entity player,
        const ::GmAdjustDebtRequest* request,
        ::GmAdjustDebtResponse* response);
    static void GmFreezeDebt(entt::entity player,
        const ::GmFreezeDebtRequest* request,
        ::GmFreezeDebtResponse* response);
    static void GmQueryDebt(entt::entity player,
        const ::GmQueryDebtRequest* request,
        ::GmQueryDebtResponse* response);
    static void GmCreateSnapshot(entt::entity player,
        const ::GmCreateSnapshotRequest* request,
        ::GmCreateSnapshotResponse* response);
    static void GmListSnapshots(entt::entity player,
        const ::GmListSnapshotsRequest* request,
        ::GmListSnapshotsResponse* response);
    static void GmPreviewRollback(entt::entity player,
        const ::GmPreviewRollbackRequest* request,
        ::GmPreviewRollbackResponse* response);
    static void GmExecuteRollback(entt::entity player,
        const ::GmExecuteRollbackRequest* request,
        ::GmExecuteRollbackResponse* response);
    static void GmQueryTransactionLog(entt::entity player,
        const ::GmQueryTransactionLogRequest* request,
        ::GmQueryTransactionLogResponse* response);
    static void GmTraceItem(entt::entity player,
        const ::GmTraceItemRequest* request,
        ::GmTraceItemResponse* response);
    static void GmClawbackItem(entt::entity player,
        const ::GmClawbackItemRequest* request,
        ::GmClawbackItemResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmAttachDebtResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmAttachDebt(player,
                static_cast<const ::GmAttachDebtRequest*>(request),
                static_cast<::GmAttachDebtResponse*>(response));
			}
            break;
        case 1:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmWaiveDebtResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmWaiveDebt(player,
                static_cast<const ::GmWaiveDebtRequest*>(request),
                static_cast<::GmWaiveDebtResponse*>(response));
			}
            break;
        case 2:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmAdjustDebtResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmAdjustDebt(player,
                static_cast<const ::GmAdjustDebtRequest*>(request),
                static_cast<::GmAdjustDebtResponse*>(response));
			}
            break;
        case 3:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmFreezeDebtResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmFreezeDebt(player,
                static_cast<const ::GmFreezeDebtRequest*>(request),
                static_cast<::GmFreezeDebtResponse*>(response));
			}
            break;
        case 4:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmQueryDebtResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmQueryDebt(player,
                static_cast<const ::GmQueryDebtRequest*>(request),
                static_cast<::GmQueryDebtResponse*>(response));
			}
            break;
        case 5:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmCreateSnapshotResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmCreateSnapshot(player,
                static_cast<const ::GmCreateSnapshotRequest*>(request),
                static_cast<::GmCreateSnapshotResponse*>(response));
			}
            break;
        case 6:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmListSnapshotsResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmListSnapshots(player,
                static_cast<const ::GmListSnapshotsRequest*>(request),
                static_cast<::GmListSnapshotsResponse*>(response));
			}
            break;
        case 7:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmPreviewRollbackResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmPreviewRollback(player,
                static_cast<const ::GmPreviewRollbackRequest*>(request),
                static_cast<::GmPreviewRollbackResponse*>(response));
			}
            break;
        case 8:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmExecuteRollbackResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmExecuteRollback(player,
                static_cast<const ::GmExecuteRollbackRequest*>(request),
                static_cast<::GmExecuteRollbackResponse*>(response));
			}
            break;
        case 9:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmQueryTransactionLogResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmQueryTransactionLog(player,
                static_cast<const ::GmQueryTransactionLogRequest*>(request),
                static_cast<::GmQueryTransactionLogResponse*>(response));
			}
            break;
        case 10:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmTraceItemResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmTraceItem(player,
                static_cast<const ::GmTraceItemRequest*>(request),
                static_cast<::GmTraceItemResponse*>(response));
			}
            break;
        case 11:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmClawbackItemResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmClawbackItem(player,
                static_cast<const ::GmClawbackItemRequest*>(request),
                static_cast<::GmClawbackItemResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
