#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/globalvariable_table.pb.h"

class GlobalVariableTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static GlobalVariableTableManager& Instance() {
        static GlobalVariableTableManager instance;
        return instance;
    }

    const GlobalVariableTableData& All() const { return data_; }

    std::pair<const GlobalVariableTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const GlobalVariableTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

private:
    LoadSuccessCallback loadSuccessCallback_;
    GlobalVariableTableData data_;
    KeyValueDataType kv_data_;
};

inline const GlobalVariableTableData& GetGlobalVariableAllTable() {
    return GlobalVariableTableManager::Instance().All();
}

#define FetchAndValidateGlobalVariableTable(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomGlobalVariableTable(prefix, tableId) \
    const auto [prefix##GlobalVariableTable, prefix##fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##GlobalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchGlobalVariableTableOrReturnCustom(tableId, customReturnValue) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchGlobalVariableTableOrReturnVoid(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return; } } while(0)

#define FetchGlobalVariableTableOrContinue(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; continue; } } while(0)

#define FetchGlobalVariableTableOrReturnFalse(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return false; } } while(0)
