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
#include "table/proto/skillpermission_table.pb.h"

class SkillPermissionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillPermissionTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        SkillPermissionTableData data;
        KeyValueDataType kvData;
        std::unordered_multimap<uint32_t, const SkillPermissionTable*> idxskill_type;
    };

    static SkillPermissionTableManager& Instance() {
        static SkillPermissionTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const SkillPermissionTableData& FindAll() const { return snapshot_->data; }

    std::pair<const SkillPermissionTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const SkillPermissionTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const SkillPermissionTable*>& GetSkill_typeIndex() const { return snapshot_->idxskill_type; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountBySkill_typeIndex(uint32_t key) const { return snapshot_->idxskill_type.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const SkillPermissionTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const SkillPermissionTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const SkillPermissionTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const SkillPermissionTable*> Where(const std::function<bool(const SkillPermissionTable&)>& pred) const {
        std::vector<const SkillPermissionTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const SkillPermissionTable* First(const std::function<bool(const SkillPermissionTable&)>& pred) const {
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

inline const SkillPermissionTableData& FindAllSkillPermissionTable() {
    return SkillPermissionTableManager::Instance().FindAll();
}

#define FetchAndValidateSkillPermissionTable(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSkillPermissionTable(prefix, tableId) \
    const auto [prefix##SkillPermissionTable, prefix##fetchResult] = SkillPermissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##SkillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSkillPermissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSkillPermissionTableOrReturnVoid(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return; } } while(0)

#define FetchSkillPermissionTableOrContinue(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; continue; } } while(0)

#define FetchSkillPermissionTableOrReturnFalse(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return false; } } while(0)
