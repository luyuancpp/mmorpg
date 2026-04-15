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
#include "table/proto/monster_table.pb.h"

class MonsterTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const MonsterTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        MonsterTableData data;
        IdMapType idMap;
    };

    static MonsterTableManager& Instance() {
        static MonsterTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const MonsterTableData& FindAll() const { return snapshot->data; }

    std::pair<const MonsterTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const MonsterTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const MonsterTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const MonsterTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const MonsterTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const MonsterTable*> Where(const std::function<bool(const MonsterTable&)>& pred) const {
        std::vector<const MonsterTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const MonsterTable* First(const std::function<bool(const MonsterTable&)>& pred) const {
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

inline const MonsterTableData& FindAllMonsterTable() {
    return MonsterTableManager::Instance().FindAll();
}

#define LookupMonster(tableId) \
    const auto [monsterRow, monsterResult] = MonsterTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(monsterRow)) { LOG_ERROR << "Monster row not found for ID: " << tableId; return monsterResult; } } while(0)

#define LookupMonsterAs(prefix, tableId) \
    const auto [prefix##MonsterRow, prefix##MonsterResult] = MonsterTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##MonsterRow)) { LOG_ERROR << "Monster row not found for ID: " << tableId; return prefix##MonsterResult; } } while(0)

#define LookupMonsterOrReturn(tableId, customReturnValue) \
    const auto [monsterRow, monsterResult] = MonsterTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(monsterRow)) { LOG_ERROR << "Monster row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupMonsterOrVoid(tableId) \
    const auto [monsterRow, monsterResult] = MonsterTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(monsterRow)) { LOG_ERROR << "Monster row not found for ID: " << tableId; return; } } while(0)

#define LookupMonsterOrContinue(tableId) \
    const auto [monsterRow, monsterResult] = MonsterTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(monsterRow)) { LOG_ERROR << "Monster row not found for ID: " << tableId; continue; } } while(0)

#define LookupMonsterOrFalse(tableId) \
    const auto [monsterRow, monsterResult] = MonsterTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(monsterRow)) { LOG_ERROR << "Monster row not found for ID: " << tableId; return false; } } while(0)
