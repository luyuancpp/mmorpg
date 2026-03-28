
#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/buff_table.pb.h"

class BuffTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const BuffTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static BuffTableManager& Instance() {
        static BuffTableManager instance;
        return instance;
    }

    const BuffTableData& All() const { return data_; }

    std::pair<const BuffTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const BuffTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }




    double GetHealth_regeneration(uint32_t tableId) {
        auto [table, ok] = GetTable(tableId);
        if (!ok || table == nullptr) {
            LOG_ERROR << "Health_regeneration table not found for ID: " << tableId;
            return double();
        }
        return expression_health_regeneration_.Value(table->health_regeneration());
    }
    void SetHealth_regenerationParam(const std::vector<double>& paramList) {
        expression_health_regeneration_.SetParam(paramList);
    }

    double GetBonus_damage(uint32_t tableId) {
        auto [table, ok] = GetTable(tableId);
        if (!ok || table == nullptr) {
            LOG_ERROR << "Bonus_damage table not found for ID: " << tableId;
            return double();
        }
        return expression_bonus_damage_.Value(table->bonus_damage());
    }
    void SetBonus_damageParam(const std::vector<double>& paramList) {
        expression_bonus_damage_.SetParam(paramList);
    }




private:
    LoadSuccessCallback loadSuccessCallback_;
    BuffTableData data_;
    KeyValueDataType kv_data_;


    ExcelExpression<double> expression_health_regeneration_;

    ExcelExpression<double> expression_bonus_damage_;

};

inline const BuffTableData& GetBuffAllTable() {
    return BuffTableManager::Instance().All();
}

#define FetchAndValidateBuffTable(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBuffTable(prefix, tableId) \
    const auto [prefix##BuffTable, prefix##fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##BuffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBuffTableOrReturnCustom(tableId, customReturnValue) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBuffTableOrReturnVoid(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return; } } while(0)

#define FetchBuffTableOrContinue(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; continue; } } while(0)

#define FetchBuffTableOrReturnFalse(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return false; } } while(0)