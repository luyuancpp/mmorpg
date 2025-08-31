#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/condition_table.pb.h"

class ConditionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ConditionTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ConditionTableManager& Instance() {
        static ConditionTableManager instance;
        return instance;
    }

    const ConditionTabledData& All() const { return data_; }

    std::pair<ConditionTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<ConditionTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ConditionTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ConditionTabledData& GetConditionAllTable() {
    return ConditionTableManager::Instance().All();
}

#define FetchAndValidateConditionTable(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomConditionTable(prefix, tableId) \
    const auto [prefix##ConditionTable, prefix##fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ConditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchConditionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchConditionTableOrReturnVoid(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return; } } while(0)

#define FetchConditionTableOrContinue(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; continue; } } while(0)

#define FetchConditionTableOrReturnFalse(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return false; } } while(0)