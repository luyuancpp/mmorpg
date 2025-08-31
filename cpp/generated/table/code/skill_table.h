#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/skill_table.pb.h"

class SkillTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static SkillTableManager& Instance() {
        static SkillTableManager instance;
        return instance;
    }

    const SkillTabledData& All() const { return data_; }

    std::pair<const SkillTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const SkillTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    double GetDamage(const uint32_t tableId){
        auto [table, ok] = GetTable(tableId);  // Fetch table using tableId
        if (!ok || table == nullptr) {         // Check if the table is valid
            LOG_ERROR << "Damage table not found for ID: " << tableId;
            return double();  // Return default value (zero) if table is invalid
        }

        // Call the appropriate method to get the damage
        return expression_damage_.Value(table->damage());
    }
    void SetDamageParam(const std::vector<double>& paramList){
               expression_damage_.SetParam(paramList);  // Set parameters for damage calculation
    }

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    SkillTabledData data_;
    KeyValueDataType kv_data_;
    ExcelExpression<double> expression_damage_;
};

inline const SkillTabledData& GetSkillAllTable() {
    return SkillTableManager::Instance().All();
}

#define FetchAndValidateSkillTable(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().GetTable(tableId); \
    do { if (!( skillTable )) { LOG_ERROR << "Skill table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSkillTable(prefix, tableId) \
    const auto [prefix##SkillTable, prefix##fetchResult] = SkillTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##SkillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSkillTableOrReturnCustom(tableId, customReturnValue) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().GetTable(tableId); \
    do { if (!( skillTable )) { LOG_ERROR << "Skill table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSkillTableOrReturnVoid(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().GetTable(tableId); \
    do { if (!( skillTable )) { LOG_ERROR << "Skill table not found for ID: " << tableId; return; } } while(0)

#define FetchSkillTableOrContinue(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().GetTable(tableId); \
    do { if (!( skillTable )) { LOG_ERROR << "Skill table not found for ID: " << tableId; continue; } } while(0)

#define FetchSkillTableOrReturnFalse(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().GetTable(tableId); \
    do { if (!( skillTable )) { LOG_ERROR << "Skill table not found for ID: " << tableId; return false; } } while(0)