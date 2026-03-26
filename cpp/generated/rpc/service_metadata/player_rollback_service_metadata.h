#pragma once
#include <cstdint>

#include "proto/scene/player_rollback.pb.h"

constexpr uint32_t SceneRollbackClientPlayerGmAttachDebtMessageId = 102;
constexpr uint32_t SceneRollbackClientPlayerGmAttachDebtIndex = 0;
#define SceneRollbackClientPlayerGmAttachDebtMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneRollbackClientPlayerGmWaiveDebtMessageId = 106;
constexpr uint32_t SceneRollbackClientPlayerGmWaiveDebtIndex = 1;
#define SceneRollbackClientPlayerGmWaiveDebtMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneRollbackClientPlayerGmAdjustDebtMessageId = 113;
constexpr uint32_t SceneRollbackClientPlayerGmAdjustDebtIndex = 2;
#define SceneRollbackClientPlayerGmAdjustDebtMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(2)

constexpr uint32_t SceneRollbackClientPlayerGmFreezeDebtMessageId = 109;
constexpr uint32_t SceneRollbackClientPlayerGmFreezeDebtIndex = 3;
#define SceneRollbackClientPlayerGmFreezeDebtMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(3)

constexpr uint32_t SceneRollbackClientPlayerGmQueryDebtMessageId = 107;
constexpr uint32_t SceneRollbackClientPlayerGmQueryDebtIndex = 4;
#define SceneRollbackClientPlayerGmQueryDebtMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(4)

constexpr uint32_t SceneRollbackClientPlayerGmCreateSnapshotMessageId = 103;
constexpr uint32_t SceneRollbackClientPlayerGmCreateSnapshotIndex = 5;
#define SceneRollbackClientPlayerGmCreateSnapshotMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(5)

constexpr uint32_t SceneRollbackClientPlayerGmListSnapshotsMessageId = 115;
constexpr uint32_t SceneRollbackClientPlayerGmListSnapshotsIndex = 6;
#define SceneRollbackClientPlayerGmListSnapshotsMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(6)

constexpr uint32_t SceneRollbackClientPlayerGmPreviewRollbackMessageId = 116;
constexpr uint32_t SceneRollbackClientPlayerGmPreviewRollbackIndex = 7;
#define SceneRollbackClientPlayerGmPreviewRollbackMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(7)

constexpr uint32_t SceneRollbackClientPlayerGmExecuteRollbackMessageId = 112;
constexpr uint32_t SceneRollbackClientPlayerGmExecuteRollbackIndex = 8;
#define SceneRollbackClientPlayerGmExecuteRollbackMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(8)

constexpr uint32_t SceneRollbackClientPlayerGmQueryTransactionLogMessageId = 117;
constexpr uint32_t SceneRollbackClientPlayerGmQueryTransactionLogIndex = 9;
#define SceneRollbackClientPlayerGmQueryTransactionLogMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(9)

constexpr uint32_t SceneRollbackClientPlayerGmTraceItemMessageId = 104;
constexpr uint32_t SceneRollbackClientPlayerGmTraceItemIndex = 10;
#define SceneRollbackClientPlayerGmTraceItemMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(10)

constexpr uint32_t SceneRollbackClientPlayerGmClawbackItemMessageId = 110;
constexpr uint32_t SceneRollbackClientPlayerGmClawbackItemIndex = 11;
#define SceneRollbackClientPlayerGmClawbackItemMethod  ::SceneRollbackClientPlayer_Stub::descriptor()->method(11)
