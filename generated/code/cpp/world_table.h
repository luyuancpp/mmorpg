#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/world_table.pb.h"

class WorldTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const WorldTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static WorldTableManager& Instance() {
        static WorldTableManager instance;
        return instance;
    }

    const WorldTableData& All() const { return data_; }

    std::pair<const WorldTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const WorldTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // FK: scene_id → BaseScene.id

private:
    LoadSuccessCallback loadSuccessCallback_;
    WorldTableData data_;
    KeyValueDataType kv_data_;
};

inline const WorldTableData& GetWorldAllTable() {
    return WorldTableManager::Instance().All();
}

#define FetchAndValidateWorldTable(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().GetTable(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomWorldTable(prefix, tableId) \
    const auto [prefix##WorldTable, prefix##fetchResult] = WorldTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##WorldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchWorldTableOrReturnCustom(tableId, customReturnValue) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().GetTable(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchWorldTableOrReturnVoid(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().GetTable(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return; } } while(0)

#define FetchWorldTableOrContinue(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().GetTable(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; continue; } } while(0)

#define FetchWorldTableOrReturnFalse(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().GetTable(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return false; } } while(0)
