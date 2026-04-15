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
#include "table/proto/cooldown_table.pb.h"

class CooldownTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const CooldownTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        CooldownTableData data;
        IdMapType idMap;
    };

    static CooldownTableManager& Instance() {
        static CooldownTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const CooldownTableData& FindAll() const { return snapshot->data; }

    std::pair<const CooldownTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const CooldownTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const CooldownTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const CooldownTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const CooldownTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const CooldownTable*> Where(const std::function<bool(const CooldownTable&)>& pred) const {
        std::vector<const CooldownTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const CooldownTable* First(const std::function<bool(const CooldownTable&)>& pred) const {
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

inline const CooldownTableData& FindAllCooldownTable() {
    return CooldownTableManager::Instance().FindAll();
}

#define LookupCooldown(tableId) \
    const auto [cooldownRow, cooldownResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownRow)) { LOG_ERROR << "Cooldown row not found for ID: " << tableId; return cooldownResult; } } while(0)

#define LookupCooldownAs(prefix, tableId) \
    const auto [prefix##CooldownRow, prefix##CooldownResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##CooldownRow)) { LOG_ERROR << "Cooldown row not found for ID: " << tableId; return prefix##CooldownResult; } } while(0)

#define LookupCooldownOrReturn(tableId, customReturnValue) \
    const auto [cooldownRow, cooldownResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownRow)) { LOG_ERROR << "Cooldown row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupCooldownOrVoid(tableId) \
    const auto [cooldownRow, cooldownResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownRow)) { LOG_ERROR << "Cooldown row not found for ID: " << tableId; return; } } while(0)

#define LookupCooldownOrContinue(tableId) \
    const auto [cooldownRow, cooldownResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownRow)) { LOG_ERROR << "Cooldown row not found for ID: " << tableId; continue; } } while(0)

#define LookupCooldownOrFalse(tableId) \
    const auto [cooldownRow, cooldownResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownRow)) { LOG_ERROR << "Cooldown row not found for ID: " << tableId; return false; } } while(0)
