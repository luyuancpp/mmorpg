#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
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

inline std::pair<const MissionTable*, uint32_t> GetMissionTable(const uint32_t keyId) { return MissionConfigurationTable::Instance().GetTable(keyId); }

inline const MissionTabledData& GetMissionAllTable() { return MissionConfigurationTable::Instance().All(); }

#define FetchAndValidateMissionTable(keyId) \
const auto [missionTable, result] = MissionConfigurationTable::Instance().GetTable(keyId); \
if (!(missionTable)) { return (result); }

#define FetchMissionTableOrReturnVoid(keyId) \
const auto [missionTable, result] = MissionConfigurationTable::Instance().GetTable(keyId); \
if (!(missionTable)) { return ;}