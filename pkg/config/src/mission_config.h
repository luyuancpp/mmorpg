#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "mission_config.pb.h"


class MissionConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const MissionTable*>;
    static MissionConfigurationTable& GetSingleton() { static MissionConfigurationTable singleton; return singleton; }
    const MissionTabledData& All() const { return data_; }
    std::pair<const MissionTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    MissionTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const MissionTable*, uint32_t> GetMissionTable(const uint32_t keyid) { return MissionConfigurationTable::GetSingleton().GetTable(keyid); }

inline const MissionTabledData& GetMissionAllTable() { return MissionConfigurationTable::GetSingleton().All(); }