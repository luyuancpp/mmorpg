#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
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

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }

    // ---- Batch Lookup (IN) ----

    std::vector<const DungeonTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const DungeonTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const DungeonTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const DungeonTable*> Filter(const std::function<bool(const DungeonTable&)>& pred) const {
        std::vector<const DungeonTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const DungeonTable* FindFirst(const std::function<bool(const DungeonTable&)>& pred) const {
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                return &data_.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

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
