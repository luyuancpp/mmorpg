#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
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

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        WorldTableData data;
        KeyValueDataType kvData;
    };

    static WorldTableManager& Instance() {
        static WorldTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const WorldTableData& FindAll() const { return snapshot_->data; }

    std::pair<const WorldTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const WorldTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // FK: scene_id → BaseScene.id

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const WorldTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const WorldTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const WorldTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const WorldTable*> Where(const std::function<bool(const WorldTable&)>& pred) const {
        std::vector<const WorldTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const WorldTable* First(const std::function<bool(const WorldTable&)>& pred) const {
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                return &snapshot_->data.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

private:
    LoadSuccessCallback loadSuccessCallback_;
    std::unique_ptr<Snapshot> snapshot_ = std::make_unique<Snapshot>();
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
