#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
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

    const WorldTableData& FindAll() const { return data_; }

    std::pair<const WorldTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const WorldTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // FK: scene_id → BaseScene.id

    // ---- Exists ----

    bool Exists(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return kv_data_.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const WorldTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const WorldTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const WorldTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const WorldTable*> Where(const std::function<bool(const WorldTable&)>& pred) const {
        std::vector<const WorldTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const WorldTable* First(const std::function<bool(const WorldTable&)>& pred) const {
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
    WorldTableData data_;
    KeyValueDataType kv_data_;
};

inline const WorldTableData& FindAllWorldTable() {
    return WorldTableManager::Instance().FindAll();
}

#define FetchAndValidateWorldTable(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomWorldTable(prefix, tableId) \
    const auto [prefix##WorldTable, prefix##fetchResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##WorldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchWorldTableOrReturnCustom(tableId, customReturnValue) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchWorldTableOrReturnVoid(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return; } } while(0)

#define FetchWorldTableOrContinue(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; continue; } } while(0)

#define FetchWorldTableOrReturnFalse(tableId) \
    const auto [worldTable, fetchResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldTable)) { LOG_ERROR << "World table not found for ID: " << tableId; return false; } } while(0)
