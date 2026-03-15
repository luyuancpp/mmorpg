#pragma once
#include <cstdint>

#include "proto/data_service/data_service.pb.h"

constexpr uint32_t DataServiceLoadPlayerDataMessageId = 90;
constexpr uint32_t DataServiceLoadPlayerDataIndex = 0;
#define DataServiceLoadPlayerDataMethod  ::DataService_Stub::descriptor()->method(0)

constexpr uint32_t DataServiceSavePlayerDataMessageId = 92;
constexpr uint32_t DataServiceSavePlayerDataIndex = 1;
#define DataServiceSavePlayerDataMethod  ::DataService_Stub::descriptor()->method(1)

constexpr uint32_t DataServiceGetPlayerFieldMessageId = 91;
constexpr uint32_t DataServiceGetPlayerFieldIndex = 2;
#define DataServiceGetPlayerFieldMethod  ::DataService_Stub::descriptor()->method(2)

constexpr uint32_t DataServiceSetPlayerFieldMessageId = 89;
constexpr uint32_t DataServiceSetPlayerFieldIndex = 3;
#define DataServiceSetPlayerFieldMethod  ::DataService_Stub::descriptor()->method(3)

constexpr uint32_t DataServiceRegisterPlayerZoneMessageId = 86;
constexpr uint32_t DataServiceRegisterPlayerZoneIndex = 4;
#define DataServiceRegisterPlayerZoneMethod  ::DataService_Stub::descriptor()->method(4)

constexpr uint32_t DataServiceGetPlayerHomeZoneMessageId = 93;
constexpr uint32_t DataServiceGetPlayerHomeZoneIndex = 5;
#define DataServiceGetPlayerHomeZoneMethod  ::DataService_Stub::descriptor()->method(5)

constexpr uint32_t DataServiceBatchGetPlayerHomeZoneMessageId = 87;
constexpr uint32_t DataServiceBatchGetPlayerHomeZoneIndex = 6;
#define DataServiceBatchGetPlayerHomeZoneMethod  ::DataService_Stub::descriptor()->method(6)

constexpr uint32_t DataServiceDeletePlayerDataMessageId = 88;
constexpr uint32_t DataServiceDeletePlayerDataIndex = 7;
#define DataServiceDeletePlayerDataMethod  ::DataService_Stub::descriptor()->method(7)
