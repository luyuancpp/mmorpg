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
    using KeyValueDataType = std::unordered_map<uint32_t, const MissionTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        MissionTableData data;
        KeyValueDataType kvData;
        std::unordered_multimap<uint32_t, const MissionTable*> idxcondition_id;
        std::unordered_multimap<uint32_t, const MissionTable*> idxnext_mission_id;
        std::unordered_multimap<uint32_t, const MissionTable*> idxtarget_count;
    };

    static MissionTableManager& Instance() {
        static MissionTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const MissionTableData& FindAll() const { return snapshot_->data; }

    std::pair<const MissionTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const MissionTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const MissionTable*>& GetCondition_idIndex() const { return snapshot_->idxcondition_id; }
    const std::unordered_multimap<uint32_t, const MissionTable*>& GetNext_mission_idIndex() const { return snapshot_->idxnext_mission_id; }
    const std::unordered_multimap<uint32_t, const MissionTable*>& GetTarget_countIndex() const { return snapshot_->idxtarget_count; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountByCondition_idIndex(uint32_t key) const { return snapshot_->idxcondition_id.count(key); }
    std::size_t CountByNext_mission_idIndex(uint32_t key) const { return snapshot_->idxnext_mission_id.count(key); }
    std::size_t CountByTarget_countIndex(uint32_t key) const { return snapshot_->idxtarget_count.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const MissionTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const MissionTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const MissionTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const MissionTable*> Where(const std::function<bool(const MissionTable&)>& pred) const {
        std::vector<const MissionTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const MissionTable* First(const std::function<bool(const MissionTable&)>& pred) const {
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

inline const MissionTableData& FindAllMissionTable() {
    return MissionTableManager::Instance().FindAll();
}

#define FetchAndValidateMissionTable(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMissionTable(prefix, tableId) \
    const auto [prefix##MissionTable, prefix##fetchResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##MissionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMissionTableOrReturnVoid(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return; } } while(0)

#define FetchMissionTableOrContinue(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; continue; } } while(0)

#define FetchMissionTableOrReturnFalse(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(missionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return false; } } while(0)
