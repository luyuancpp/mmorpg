
#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/mission_table.pb.h"

class MissionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MissionTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static MissionTableManager& Instance() {
        static MissionTableManager instance;
        return instance;
    }

    const MissionTableData& All() const { return data_; }

    std::pair<const MissionTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MissionTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }







private:
    LoadSuccessCallback loadSuccessCallback_;
    MissionTableData data_;
    KeyValueDataType kv_data_;


};

inline const MissionTableData& GetMissionAllTable() {
    return MissionTableManager::Instance().All();
}

#define FetchAndValidateMissionTable(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMissionTable(prefix, tableId) \
    const auto [prefix##MissionTable, prefix##fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MissionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMissionTableOrReturnVoid(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return; } } while(0)

#define FetchMissionTableOrContinue(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; continue; } } while(0)

#define FetchMissionTableOrReturnFalse(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return false; } } while(0)