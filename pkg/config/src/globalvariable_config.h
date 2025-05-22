#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "globalvariable_config.pb.h"

class GlobalVariableConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static GlobalVariableConfigurationTable& Instance() {
        static GlobalVariableConfigurationTable instance;
        return instance;
    }

    const GlobalVariableTabledData& All() const { return data_; }

    std::pair<const GlobalVariableTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const GlobalVariableTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    GlobalVariableTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const GlobalVariableTabledData& GetGlobalVariableAllTable() {
    return GlobalVariableConfigurationTable::Instance().All();
}

#define FetchAndValidateGlobalVariableTable(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomGlobalVariableTable(prefix, tableId) \
    const auto [prefix##GlobalVariableTable, prefix##fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##GlobalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchGlobalVariableTableOrReturnCustom(tableId, customReturnValue) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchGlobalVariableTableOrReturnVoid(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return; } } while(0)

#define FetchGlobalVariableTableOrContinue(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; continue; } } while(0)

#define FetchGlobalVariableTableOrReturnFalse(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return false; } } while(0)