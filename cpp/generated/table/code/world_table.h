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
    using IdMapType = std::unordered_map<uint32_t, const WorldTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        WorldTableData data;
        IdMapType idMap;
    };

    static WorldTableManager& Instance() {
        static WorldTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const WorldTableData& FindAll() const { return snapshot->data; }

    std::pair<const WorldTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const WorldTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    // FK: scene_id -> BaseScene.id

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const WorldTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const WorldTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const WorldTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const WorldTable*> Where(const std::function<bool(const WorldTable&)>& pred) const {
        std::vector<const WorldTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const WorldTable* First(const std::function<bool(const WorldTable&)>& pred) const {
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                return &snapshot->data.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

private:
    LoadSuccessCallback loadSuccessCallback;
    std::unique_ptr<Snapshot> snapshot = std::make_unique<Snapshot>();
};

inline const WorldTableData& FindAllWorldTable() {
    return WorldTableManager::Instance().FindAll();
}

#define LookupWorld(tableId) \
    const auto [worldRow, worldResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldRow)) { LOG_ERROR << "World row not found for ID: " << tableId; return worldResult; } } while(0)

#define LookupWorldAs(prefix, tableId) \
    const auto [prefix##WorldRow, prefix##WorldResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##WorldRow)) { LOG_ERROR << "World row not found for ID: " << tableId; return prefix##WorldResult; } } while(0)

#define LookupWorldOrReturn(tableId, customReturnValue) \
    const auto [worldRow, worldResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldRow)) { LOG_ERROR << "World row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupWorldOrVoid(tableId) \
    const auto [worldRow, worldResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldRow)) { LOG_ERROR << "World row not found for ID: " << tableId; return; } } while(0)

#define LookupWorldOrContinue(tableId) \
    const auto [worldRow, worldResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldRow)) { LOG_ERROR << "World row not found for ID: " << tableId; continue; } } while(0)

#define LookupWorldOrFalse(tableId) \
    const auto [worldRow, worldResult] = WorldTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(worldRow)) { LOG_ERROR << "World row not found for ID: " << tableId; return false; } } while(0)
