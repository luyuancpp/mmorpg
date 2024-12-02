#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "mission_config.pb.h"


class MissionConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MissionTable*>;
    static MissionConfigurationTable& Instance() { static MissionConfigurationTable instance; return instance; }
    const MissionTabledData& All() const { return data_; }
    std::pair<const MissionTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    MissionTabledData data_;
    KeyValueDataType kv_data_;

};

inline const MissionTabledData& GetMissionAllTable() { return MissionConfigurationTable::Instance().All(); }

#define FetchAndValidateMissionTable(keyId) \
const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomMissionTable(prefix, keyId) \
const auto [##prefix##MissionTable, prefix##fetchResult] = MissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##MissionTable)) { LOG_ERROR << "Mission table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchMissionTableOrReturnCustom(keyId, customReturnValue) \
const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchMissionTableOrReturnVoid(keyId) \
const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << keyId;return ;}} while (0)

#define FetchMissionTableOrContinue(keyId) \
const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(keyId); \
do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << keyId;continue; }} while (0)

#define FetchMissionTableOrReturnFalse(keyId) \
const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << keyId;return false; }} while (0)