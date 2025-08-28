#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/buff_table.pb.h"

class BuffConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const BuffTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static BuffConfigurationTable& Instance() {
        static BuffConfigurationTable instance;
        return instance;
    }

    const BuffTabledData& All() const { return data_; }

    std::pair<const BuffTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const BuffTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    double GetHealthregeneration(const uint32_t tableId){
        auto [table, ok] = GetTable(tableId);  // Fetch table using tableId
        if (!ok || table == nullptr) {         // Check if the table is valid
            LOG_ERROR << "Healthregeneration table not found for ID: " << tableId;
            return double();  // Return default value (zero) if table is invalid
        }

        // Call the appropriate method to get the damage
        return expression_healthregeneration_.Value(table->healthregeneration());
    }
    void SetHealthregenerationParam(const std::vector<double>& paramList){
               expression_healthregeneration_.SetParam(paramList);  // Set parameters for damage calculation
    }double GetBonusdamage(const uint32_t tableId){
        auto [table, ok] = GetTable(tableId);  // Fetch table using tableId
        if (!ok || table == nullptr) {         // Check if the table is valid
            LOG_ERROR << "Bonusdamage table not found for ID: " << tableId;
            return double();  // Return default value (zero) if table is invalid
        }

        // Call the appropriate method to get the damage
        return expression_bonusdamage_.Value(table->bonusdamage());
    }
    void SetBonusdamageParam(const std::vector<double>& paramList){
               expression_bonusdamage_.SetParam(paramList);  // Set parameters for damage calculation
    }

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    BuffTabledData data_;
    KeyValueDataType kv_data_;
    ExcelExpression<double> expression_healthregeneration_;ExcelExpression<double> expression_bonusdamage_;
};

inline const BuffTabledData& GetBuffAllTable() {
    return BuffConfigurationTable::Instance().All();
}

#define FetchAndValidateBuffTable(tableId) \
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBuffTable(prefix, tableId) \
    const auto [prefix##BuffTable, prefix##fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##BuffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBuffTableOrReturnCustom(tableId, customReturnValue) \
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBuffTableOrReturnVoid(tableId) \
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return; } } while(0)

#define FetchBuffTableOrContinue(tableId) \
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; continue; } } while(0)

#define FetchBuffTableOrReturnFalse(tableId) \
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return false; } } while(0)