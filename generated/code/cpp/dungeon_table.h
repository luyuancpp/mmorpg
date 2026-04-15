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
#include "table/proto/dungeon_table.pb.h"

class DungeonTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const DungeonTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        DungeonTableData data;
        IdMapType idMap;
        std::unordered_multimap<uint32_t, const DungeonTable*> sceneIdIndex;
    };

    static DungeonTableManager& Instance() {
        static DungeonTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const DungeonTableData& FindAll() const { return snapshot->data; }

    std::pair<const DungeonTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const DungeonTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    // FK: scene_id -> BaseScene.id
    const std::unordered_multimap<uint32_t, const DungeonTable*>& GetSceneIdIndex() const { return snapshot->sceneIdIndex; }
    std::vector<const DungeonTable*> GetBySceneId(uint32_t key) const {
        auto range = snapshot->sceneIdIndex.equal_range(key);
        std::vector<const DungeonTable*> result;
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        return result;
    }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }
    std::size_t CountBySceneIdIndex(uint32_t key) const { return snapshot->sceneIdIndex.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const DungeonTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const DungeonTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const DungeonTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const DungeonTable*> Where(const std::function<bool(const DungeonTable&)>& pred) const {
        std::vector<const DungeonTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const DungeonTable* First(const std::function<bool(const DungeonTable&)>& pred) const {
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

inline const DungeonTableData& FindAllDungeonTable() {
    return DungeonTableManager::Instance().FindAll();
}

#define LookupDungeon(tableId) \
    const auto [dungeonRow, dungeonResult] = DungeonTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(dungeonRow)) { LOG_ERROR << "Dungeon row not found for ID: " << tableId; return dungeonResult; } } while(0)

#define LookupDungeonAs(prefix, tableId) \
    const auto [prefix##DungeonRow, prefix##DungeonResult] = DungeonTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##DungeonRow)) { LOG_ERROR << "Dungeon row not found for ID: " << tableId; return prefix##DungeonResult; } } while(0)

#define LookupDungeonOrReturn(tableId, customReturnValue) \
    const auto [dungeonRow, dungeonResult] = DungeonTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(dungeonRow)) { LOG_ERROR << "Dungeon row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupDungeonOrVoid(tableId) \
    const auto [dungeonRow, dungeonResult] = DungeonTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(dungeonRow)) { LOG_ERROR << "Dungeon row not found for ID: " << tableId; return; } } while(0)

#define LookupDungeonOrContinue(tableId) \
    const auto [dungeonRow, dungeonResult] = DungeonTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(dungeonRow)) { LOG_ERROR << "Dungeon row not found for ID: " << tableId; continue; } } while(0)

#define LookupDungeonOrFalse(tableId) \
    const auto [dungeonRow, dungeonResult] = DungeonTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(dungeonRow)) { LOG_ERROR << "Dungeon row not found for ID: " << tableId; return false; } } while(0)
