#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
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
