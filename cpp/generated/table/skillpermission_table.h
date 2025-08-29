#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "type_define/warn_on_save_ptr.h"
#include "proto/table/skillpermission_table.pb.h"

using SkillPermissionTableTempPtr = WarnOnSavePtr<const SkillPermissionTable>;

class SkillPermissionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillPermissionTableTempPtr>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static SkillPermissionTableManager& Instance() {
        static SkillPermissionTableManager instance;
        return instance;
    }

    const SkillPermissionTabledData& All() const { return data_; }

    std::pair<const SkillPermissionTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<const SkillPermissionTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    SkillPermissionTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const SkillPermissionTabledData& GetSkillPermissionAllTable() {
    return SkillPermissionTableManager::Instance().All();
}

#define FetchAndValidateSkillPermissionTable(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSkillPermissionTable(prefix, tableId) \
    const auto [prefix##SkillPermissionTable, prefix##fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##SkillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSkillPermissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSkillPermissionTableOrReturnVoid(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return; } } while(0)

#define FetchSkillPermissionTableOrContinue(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; continue; } } while(0)

#define FetchSkillPermissionTableOrReturnFalse(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionTableManager::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return false; } } while(0)