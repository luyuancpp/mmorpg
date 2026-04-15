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
#include "table/proto/mission_table.pb.h"

class MissionTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const MissionTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        MissionTableData data;
        IdMapType idMap;
        std::unordered_multimap<uint32_t, const MissionTable*> conditionIdIndex;
        std::unordered_multimap<uint32_t, const MissionTable*> nextMissionIdIndex;
        std::unordered_multimap<uint32_t, const MissionTable*> targetCountIndex;
    };

    static MissionTableManager& Instance() {
        static MissionTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const MissionTableData& FindAll() const { return snapshot->data; }

    std::pair<const MissionTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const MissionTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    const std::unordered_multimap<uint32_t, const MissionTable*>& GetConditionIdIndex() const { return snapshot->conditionIdIndex; }
    const std::unordered_multimap<uint32_t, const MissionTable*>& GetNextMissionIdIndex() const { return snapshot->nextMissionIdIndex; }
    const std::unordered_multimap<uint32_t, const MissionTable*>& GetTargetCountIndex() const { return snapshot->targetCountIndex; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }
    std::size_t CountByConditionIdIndex(uint32_t key) const { return snapshot->conditionIdIndex.count(key); }
    std::size_t CountByNextMissionIdIndex(uint32_t key) const { return snapshot->nextMissionIdIndex.count(key); }
    std::size_t CountByTargetCountIndex(uint32_t key) const { return snapshot->targetCountIndex.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const MissionTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const MissionTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const MissionTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const MissionTable*> Where(const std::function<bool(const MissionTable&)>& pred) const {
        std::vector<const MissionTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const MissionTable* First(const std::function<bool(const MissionTable&)>& pred) const {
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

inline const MissionTableData& FindAllMissionTable() {
    return MissionTableManager::Instance().FindAll();
}

#define LookupMission(tableId) \
    const auto [missionRow, missionResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionRow)) { LOG_ERROR << "Mission row not found for ID: " << tableId; return missionResult; } } while(0)

#define LookupMissionAs(prefix, tableId) \
    const auto [prefix##MissionRow, prefix##MissionResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##MissionRow)) { LOG_ERROR << "Mission row not found for ID: " << tableId; return prefix##MissionResult; } } while(0)

#define LookupMissionOrReturn(tableId, customReturnValue) \
    const auto [missionRow, missionResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionRow)) { LOG_ERROR << "Mission row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupMissionOrVoid(tableId) \
    const auto [missionRow, missionResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionRow)) { LOG_ERROR << "Mission row not found for ID: " << tableId; return; } } while(0)

#define LookupMissionOrContinue(tableId) \
    const auto [missionRow, missionResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionRow)) { LOG_ERROR << "Mission row not found for ID: " << tableId; continue; } } while(0)

#define LookupMissionOrFalse(tableId) \
    const auto [missionRow, missionResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionRow)) { LOG_ERROR << "Mission row not found for ID: " << tableId; return false; } } while(0)
