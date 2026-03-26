#pragma once

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
            GmAttachDebt(player,
                static_cast<const ::GmAttachDebtRequest*>(request),
                static_cast<::GmAttachDebtResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmAttachDebtResponse*>(response));
			}
            break;
        case 1:
			{
            GmWaiveDebt(player,
                static_cast<const ::GmWaiveDebtRequest*>(request),
                static_cast<::GmWaiveDebtResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmWaiveDebtResponse*>(response));
			}
            break;
        case 2:
			{
            GmAdjustDebt(player,
                static_cast<const ::GmAdjustDebtRequest*>(request),
                static_cast<::GmAdjustDebtResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmAdjustDebtResponse*>(response));
			}
            break;
        case 3:
			{
            GmFreezeDebt(player,
                static_cast<const ::GmFreezeDebtRequest*>(request),
                static_cast<::GmFreezeDebtResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmFreezeDebtResponse*>(response));
			}
            break;
        case 4:
			{
            GmQueryDebt(player,
                static_cast<const ::GmQueryDebtRequest*>(request),
                static_cast<::GmQueryDebtResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmQueryDebtResponse*>(response));
			}
            break;
        case 5:
			{
            GmCreateSnapshot(player,
                static_cast<const ::GmCreateSnapshotRequest*>(request),
                static_cast<::GmCreateSnapshotResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmCreateSnapshotResponse*>(response));
			}
            break;
        case 6:
			{
            GmListSnapshots(player,
                static_cast<const ::GmListSnapshotsRequest*>(request),
                static_cast<::GmListSnapshotsResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmListSnapshotsResponse*>(response));
			}
            break;
        case 7:
			{
            GmPreviewRollback(player,
                static_cast<const ::GmPreviewRollbackRequest*>(request),
                static_cast<::GmPreviewRollbackResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmPreviewRollbackResponse*>(response));
			}
            break;
        case 8:
			{
            GmExecuteRollback(player,
                static_cast<const ::GmExecuteRollbackRequest*>(request),
                static_cast<::GmExecuteRollbackResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmExecuteRollbackResponse*>(response));
			}
            break;
        case 9:
			{
            GmQueryTransactionLog(player,
                static_cast<const ::GmQueryTransactionLogRequest*>(request),
                static_cast<::GmQueryTransactionLogResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmQueryTransactionLogResponse*>(response));
			}
            break;
        case 10:
			{
            GmTraceItem(player,
                static_cast<const ::GmTraceItemRequest*>(request),
                static_cast<::GmTraceItemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmTraceItemResponse*>(response));
			}
            break;
        case 11:
			{
            GmClawbackItem(player,
                static_cast<const ::GmClawbackItemRequest*>(request),
                static_cast<::GmClawbackItemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmClawbackItemResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
