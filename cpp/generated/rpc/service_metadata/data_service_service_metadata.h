#pragma once
#include <cstdint>

#include "proto/data_service/data_service.pb.h"

constexpr uint32_t DataServiceReadAccountDirectoryMessageId = 318;
constexpr uint32_t DataServiceReadAccountDirectoryIndex = 0;
#define DataServiceReadAccountDirectoryMethod  ::DataService_Stub::descriptor()->method(0)

constexpr uint32_t DataServiceCreateAccountRoleMessageId = 319;
constexpr uint32_t DataServiceCreateAccountRoleIndex = 1;
#define DataServiceCreateAccountRoleMethod  ::DataService_Stub::descriptor()->method(1)

constexpr uint32_t DataServiceLookupAccountRoleMessageId = 320;
constexpr uint32_t DataServiceLookupAccountRoleIndex = 2;
#define DataServiceLookupAccountRoleMethod  ::DataService_Stub::descriptor()->method(2)

constexpr uint32_t DataServiceRepairAccountRoleMessageId = 321;
constexpr uint32_t DataServiceRepairAccountRoleIndex = 3;
#define DataServiceRepairAccountRoleMethod  ::DataService_Stub::descriptor()->method(3)

constexpr uint32_t DataServiceRemoveAccountRolesMessageId = 322;
constexpr uint32_t DataServiceRemoveAccountRolesIndex = 4;
#define DataServiceRemoveAccountRolesMethod  ::DataService_Stub::descriptor()->method(4)

constexpr uint32_t DataServiceBeginAccountRoleTradeMessageId = 323;
constexpr uint32_t DataServiceBeginAccountRoleTradeIndex = 5;
#define DataServiceBeginAccountRoleTradeMethod  ::DataService_Stub::descriptor()->method(5)

constexpr uint32_t DataServiceConfirmAccountRoleTradeFreezeMessageId = 324;
constexpr uint32_t DataServiceConfirmAccountRoleTradeFreezeIndex = 6;
#define DataServiceConfirmAccountRoleTradeFreezeMethod  ::DataService_Stub::descriptor()->method(6)

constexpr uint32_t DataServiceCancelAccountRoleTradeMessageId = 325;
constexpr uint32_t DataServiceCancelAccountRoleTradeIndex = 7;
#define DataServiceCancelAccountRoleTradeMethod  ::DataService_Stub::descriptor()->method(7)

constexpr uint32_t DataServiceTransferAccountRoleMessageId = 326;
constexpr uint32_t DataServiceTransferAccountRoleIndex = 8;
#define DataServiceTransferAccountRoleMethod  ::DataService_Stub::descriptor()->method(8)

constexpr uint32_t DataServiceRegisterRoleAdmissionMessageId = 327;
constexpr uint32_t DataServiceRegisterRoleAdmissionIndex = 9;
#define DataServiceRegisterRoleAdmissionMethod  ::DataService_Stub::descriptor()->method(9)

constexpr uint32_t DataServiceCloseRoleAdmissionsMessageId = 328;
constexpr uint32_t DataServiceCloseRoleAdmissionsIndex = 10;
#define DataServiceCloseRoleAdmissionsMethod  ::DataService_Stub::descriptor()->method(10)

constexpr uint32_t DataServiceAckRoleTradeDrainMessageId = 329;
constexpr uint32_t DataServiceAckRoleTradeDrainIndex = 11;
#define DataServiceAckRoleTradeDrainMethod  ::DataService_Stub::descriptor()->method(11)

constexpr uint32_t DataServiceLoadPlayerDataMessageId = 90;
constexpr uint32_t DataServiceLoadPlayerDataIndex = 12;
#define DataServiceLoadPlayerDataMethod  ::DataService_Stub::descriptor()->method(12)

constexpr uint32_t DataServiceSavePlayerDataMessageId = 92;
constexpr uint32_t DataServiceSavePlayerDataIndex = 13;
#define DataServiceSavePlayerDataMethod  ::DataService_Stub::descriptor()->method(13)

constexpr uint32_t DataServiceGetPlayerFieldMessageId = 91;
constexpr uint32_t DataServiceGetPlayerFieldIndex = 14;
#define DataServiceGetPlayerFieldMethod  ::DataService_Stub::descriptor()->method(14)

constexpr uint32_t DataServiceSetPlayerFieldMessageId = 89;
constexpr uint32_t DataServiceSetPlayerFieldIndex = 15;
#define DataServiceSetPlayerFieldMethod  ::DataService_Stub::descriptor()->method(15)

constexpr uint32_t DataServiceRegisterPlayerZoneMessageId = 86;
constexpr uint32_t DataServiceRegisterPlayerZoneIndex = 16;
#define DataServiceRegisterPlayerZoneMethod  ::DataService_Stub::descriptor()->method(16)

constexpr uint32_t DataServiceGetPlayerHomeZoneMessageId = 93;
constexpr uint32_t DataServiceGetPlayerHomeZoneIndex = 17;
#define DataServiceGetPlayerHomeZoneMethod  ::DataService_Stub::descriptor()->method(17)

constexpr uint32_t DataServiceBatchGetPlayerHomeZoneMessageId = 87;
constexpr uint32_t DataServiceBatchGetPlayerHomeZoneIndex = 18;
#define DataServiceBatchGetPlayerHomeZoneMethod  ::DataService_Stub::descriptor()->method(18)

constexpr uint32_t DataServiceRemapHomeZoneForMergeMessageId = 129;
constexpr uint32_t DataServiceRemapHomeZoneForMergeIndex = 19;
#define DataServiceRemapHomeZoneForMergeMethod  ::DataService_Stub::descriptor()->method(19)

constexpr uint32_t DataServiceDeletePlayerDataMessageId = 88;
constexpr uint32_t DataServiceDeletePlayerDataIndex = 20;
#define DataServiceDeletePlayerDataMethod  ::DataService_Stub::descriptor()->method(20)

constexpr uint32_t DataServiceCreatePlayerSnapshotMessageId = 96;
constexpr uint32_t DataServiceCreatePlayerSnapshotIndex = 21;
#define DataServiceCreatePlayerSnapshotMethod  ::DataService_Stub::descriptor()->method(21)

constexpr uint32_t DataServiceListPlayerSnapshotsMessageId = 97;
constexpr uint32_t DataServiceListPlayerSnapshotsIndex = 22;
#define DataServiceListPlayerSnapshotsMethod  ::DataService_Stub::descriptor()->method(22)

constexpr uint32_t DataServiceGetPlayerSnapshotDiffMessageId = 98;
constexpr uint32_t DataServiceGetPlayerSnapshotDiffIndex = 23;
#define DataServiceGetPlayerSnapshotDiffMethod  ::DataService_Stub::descriptor()->method(23)

constexpr uint32_t DataServiceRollbackPlayerMessageId = 99;
constexpr uint32_t DataServiceRollbackPlayerIndex = 24;
#define DataServiceRollbackPlayerMethod  ::DataService_Stub::descriptor()->method(24)

constexpr uint32_t DataServiceRollbackZoneMessageId = 100;
constexpr uint32_t DataServiceRollbackZoneIndex = 25;
#define DataServiceRollbackZoneMethod  ::DataService_Stub::descriptor()->method(25)

constexpr uint32_t DataServiceRollbackAllMessageId = 101;
constexpr uint32_t DataServiceRollbackAllIndex = 26;
#define DataServiceRollbackAllMethod  ::DataService_Stub::descriptor()->method(26)

constexpr uint32_t DataServiceBatchRecallItemsMessageId = 108;
constexpr uint32_t DataServiceBatchRecallItemsIndex = 27;
#define DataServiceBatchRecallItemsMethod  ::DataService_Stub::descriptor()->method(27)

constexpr uint32_t DataServiceQueryTransactionLogMessageId = 114;
constexpr uint32_t DataServiceQueryTransactionLogIndex = 28;
#define DataServiceQueryTransactionLogMethod  ::DataService_Stub::descriptor()->method(28)

constexpr uint32_t DataServiceCreateEventSnapshotMessageId = 105;
constexpr uint32_t DataServiceCreateEventSnapshotIndex = 29;
#define DataServiceCreateEventSnapshotMethod  ::DataService_Stub::descriptor()->method(29)

constexpr uint32_t DataServiceAllocateIdSegmentMessageId = 180;
constexpr uint32_t DataServiceAllocateIdSegmentIndex = 30;
#define DataServiceAllocateIdSegmentMethod  ::DataService_Stub::descriptor()->method(30)

constexpr uint32_t DataServiceReservePlayerNameMessageId = 229;
constexpr uint32_t DataServiceReservePlayerNameIndex = 31;
#define DataServiceReservePlayerNameMethod  ::DataService_Stub::descriptor()->method(31)

constexpr uint32_t DataServiceReleasePlayerNameMessageId = 237;
constexpr uint32_t DataServiceReleasePlayerNameIndex = 32;
#define DataServiceReleasePlayerNameMethod  ::DataService_Stub::descriptor()->method(32)

constexpr uint32_t DataServiceBatchGetPlayerNameMessageId = 231;
constexpr uint32_t DataServiceBatchGetPlayerNameIndex = 33;
#define DataServiceBatchGetPlayerNameMethod  ::DataService_Stub::descriptor()->method(33)

constexpr uint32_t DataServiceGetPlayerAssetOpLedgerMessageId = 248;
constexpr uint32_t DataServiceGetPlayerAssetOpLedgerIndex = 34;
#define DataServiceGetPlayerAssetOpLedgerMethod  ::DataService_Stub::descriptor()->method(34)

constexpr uint32_t DataServiceGetPlayerInboxLedgerMessageId = 258;
constexpr uint32_t DataServiceGetPlayerInboxLedgerIndex = 35;
#define DataServiceGetPlayerInboxLedgerMethod  ::DataService_Stub::descriptor()->method(35)
