#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/dungeon_table.pb.h"

class DungeonTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const DungeonTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static DungeonTableManager& Instance() {
        static DungeonTableManager instance;
        return instance;
    }

    const DungeonTableData& All() const { return data_; }

    std::pair<const DungeonTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const DungeonTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // FK: scene_id → BaseScene.id

private:
    LoadSuccessCallback loadSuccessCallback_;
    DungeonTableData data_;
    KeyValueDataType kv_data_;
};

inline const DungeonTableData& GetDungeonAllTable() {
    return DungeonTableManager::Instance().All();
}

#define FetchAndValidateDungeonTable(tableId) \
    const auto [dungeonTable, fetchResult] = DungeonTableManager::Instance().GetTable(tableId); \
    do { if (!(dungeonTable)) { LOG_ERROR << "Dungeon table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomDungeonTable(prefix, tableId) \
    const auto [prefix##DungeonTable, prefix##fetchResult] = DungeonTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##DungeonTable)) { LOG_ERROR << "Dungeon table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchDungeonTableOrReturnCustom(tableId, customReturnValue) \
    const auto [dungeonTable, fetchResult] = DungeonTableManager::Instance().GetTable(tableId); \
    do { if (!(dungeonTable)) { LOG_ERROR << "Dungeon table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchDungeonTableOrReturnVoid(tableId) \
    const auto [dungeonTable, fetchResult] = DungeonTableManager::Instance().GetTable(tableId); \
    do { if (!(dungeonTable)) { LOG_ERROR << "Dungeon table not found for ID: " << tableId; return; } } while(0)

#define FetchDungeonTableOrContinue(tableId) \
    const auto [dungeonTable, fetchResult] = DungeonTableManager::Instance().GetTable(tableId); \
    do { if (!(dungeonTable)) { LOG_ERROR << "Dungeon table not found for ID: " << tableId; continue; } } while(0)

#define FetchDungeonTableOrReturnFalse(tableId) \
    const auto [dungeonTable, fetchResult] = DungeonTableManager::Instance().GetTable(tableId); \
    do { if (!(dungeonTable)) { LOG_ERROR << "Dungeon table not found for ID: " << tableId; return false; } } while(0)
