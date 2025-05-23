#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "skillpermission_config.pb.h"

class SkillPermissionConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillPermissionTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static SkillPermissionConfigurationTable& Instance() {
        static SkillPermissionConfigurationTable instance;
        return instance;
    }

    const SkillPermissionTabledData& All() const { return data_; }

    std::pair<const SkillPermissionTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const SkillPermissionTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
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
    return SkillPermissionConfigurationTable::Instance().All();
}

#define FetchAndValidateSkillPermissionTable(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSkillPermissionTable(prefix, tableId) \
    const auto [prefix##SkillPermissionTable, prefix##fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##SkillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSkillPermissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSkillPermissionTableOrReturnVoid(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return; } } while(0)

#define FetchSkillPermissionTableOrContinue(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; continue; } } while(0)

#define FetchSkillPermissionTableOrReturnFalse(tableId) \
    const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( skillPermissionTable )) { LOG_ERROR << "SkillPermission table not found for ID: " << tableId; return false; } } while(0)