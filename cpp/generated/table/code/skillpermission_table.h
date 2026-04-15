#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
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

    static SkillPermissionTableManager& Instance() {
        static SkillPermissionTableManager instance;
        return instance;
    }

    const SkillPermissionTableData& All() const { return data_; }

    std::pair<const SkillPermissionTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const SkillPermissionTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const SkillPermissionTable*>& GetSkill_typeIndex() const { return idx_skill_type_; }

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }
    std::size_t CountBySkill_typeIndex(uint32_t key) const { return idx_skill_type_.count(key); }

    // ---- Batch Lookup (IN) ----

    std::vector<const SkillPermissionTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const SkillPermissionTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const SkillPermissionTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const SkillPermissionTable*> Filter(const std::function<bool(const SkillPermissionTable&)>& pred) const {
        std::vector<const SkillPermissionTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const SkillPermissionTable* FindFirst(const std::function<bool(const SkillPermissionTable&)>& pred) const {
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
    SkillPermissionTableData data_;
    KeyValueDataType kv_data_;
    std::unordered_multimap<uint32_t, const SkillPermissionTable*> idx_skill_type_;
};

inline const SkillPermissionTableData& GetSkillPermissionAllTable() {
    return SkillPermissionTableManager::Instance().All();
}

#define FetchAndValidateSkillPermissionTable(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSkillPermissionTable(prefix, tableId) \
    const auto [prefix##SkillPermissionTable, prefix##fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##SkillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSkillPermissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSkillPermissionTableOrReturnVoid(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return; } } while(0)

#define FetchSkillPermissionTableOrContinue(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; continue; } } while(0)

#define FetchSkillPermissionTableOrReturnFalse(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return false; } } while(0)
