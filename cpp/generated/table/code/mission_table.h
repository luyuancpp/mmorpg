#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/mission_table.pb.h"

class MissionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MissionTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static MissionTableManager& Instance() {
        static MissionTableManager instance;
        return instance;
    }

    const MissionTableData& All() const { return data_; }

    std::pair<const MissionTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MissionTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const MissionTable*>& GetCondition_idIndex() const { return idx_condition_id_; }
    const std::unordered_multimap<uint32_t, const MissionTable*>& GetNext_mission_idIndex() const { return idx_next_mission_id_; }
    const std::unordered_multimap<uint32_t, const MissionTable*>& GetTarget_countIndex() const { return idx_target_count_; }

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }
    std::size_t CountByCondition_idIndex(uint32_t key) const { return idx_condition_id_.count(key); }
    std::size_t CountByNext_mission_idIndex(uint32_t key) const { return idx_next_mission_id_.count(key); }
    std::size_t CountByTarget_countIndex(uint32_t key) const { return idx_target_count_.count(key); }

    // ---- Batch Lookup (IN) ----

    std::vector<const MissionTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const MissionTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const MissionTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const MissionTable*> Filter(const std::function<bool(const MissionTable&)>& pred) const {
        std::vector<const MissionTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const MissionTable* FindFirst(const std::function<bool(const MissionTable&)>& pred) const {
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
    MissionTableData data_;
    KeyValueDataType kv_data_;
    std::unordered_multimap<uint32_t, const MissionTable*> idx_condition_id_;
    std::unordered_multimap<uint32_t, const MissionTable*> idx_next_mission_id_;
    std::unordered_multimap<uint32_t, const MissionTable*> idx_target_count_;
};

inline const MissionTableData& GetMissionAllTable() {
    return MissionTableManager::Instance().All();
}

#define FetchAndValidateMissionTable(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMissionTable(prefix, tableId) \
    const auto [prefix##MissionTable, prefix##fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MissionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMissionTableOrReturnVoid(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return; } } while(0)

#define FetchMissionTableOrContinue(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; continue; } } while(0)

#define FetchMissionTableOrReturnFalse(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return false; } } while(0)
