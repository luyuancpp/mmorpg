
#pragma once
#include "proto/scene/player_rollback.pb.h"

#include "rpc/player_rpc_response_handler.h"

class SceneRollbackClientPlayerReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;

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
            GmAttachDebt(player,
                nullptr,
                static_cast<::GmAttachDebtResponse*>(response));
            break;
        case 1:
            GmWaiveDebt(player,
                nullptr,
                static_cast<::GmWaiveDebtResponse*>(response));
            break;
        case 2:
            GmAdjustDebt(player,
                nullptr,
                static_cast<::GmAdjustDebtResponse*>(response));
            break;
        case 3:
            GmFreezeDebt(player,
                nullptr,
                static_cast<::GmFreezeDebtResponse*>(response));
            break;
        case 4:
            GmQueryDebt(player,
                nullptr,
                static_cast<::GmQueryDebtResponse*>(response));
            break;
        case 5:
            GmCreateSnapshot(player,
                nullptr,
                static_cast<::GmCreateSnapshotResponse*>(response));
            break;
        case 6:
            GmListSnapshots(player,
                nullptr,
                static_cast<::GmListSnapshotsResponse*>(response));
            break;
        case 7:
            GmPreviewRollback(player,
                nullptr,
                static_cast<::GmPreviewRollbackResponse*>(response));
            break;
        case 8:
            GmExecuteRollback(player,
                nullptr,
                static_cast<::GmExecuteRollbackResponse*>(response));
            break;
        case 9:
            GmQueryTransactionLog(player,
                nullptr,
                static_cast<::GmQueryTransactionLogResponse*>(response));
            break;
        case 10:
            GmTraceItem(player,
                nullptr,
                static_cast<::GmTraceItemResponse*>(response));
            break;
        case 11:
            GmClawbackItem(player,
                nullptr,
                static_cast<::GmClawbackItemResponse*>(response));
            break;
        default:
            break;
        }
    }

};
