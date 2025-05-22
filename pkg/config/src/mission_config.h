#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "mission_config.pb.h"

class MissionConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MissionTable*>;

    static MissionConfigurationTable& Instance() {
        static MissionConfigurationTable instance;
        return instance;
    }

    const MissionTabledData& All() const { return data_; }
    std::pair<const MissionTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MissionTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    MissionTabledData data_;
    KeyValueDataType kv_data_;
};

inline const MissionTabledData& GetMissionAllTable() {
    return MissionConfigurationTable::Instance().All();
}

#define FetchAndValidateMissionTable(tableId) \
    const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMissionTable(prefix, tableId) \
    const auto [prefix##MissionTable, prefix##fetchResult] = MissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##MissionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMissionTableOrReturnVoid(tableId) \
    const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return; } } while(0)

#define FetchMissionTableOrContinue(tableId) \
    const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; continue; } } while(0)

#define FetchMissionTableOrReturnFalse(tableId) \
    const auto [missionTable, fetchResult] = MissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return false; } } while(0)