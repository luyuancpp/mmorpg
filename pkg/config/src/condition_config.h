#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "condition_config.pb.h"


class ConditionConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ConditionTable*>;
    static ConditionConfigurationTable& Instance() { static ConditionConfigurationTable instance; return instance; }
    const ConditionTabledData& All() const { return data_; }
    std::pair<const ConditionTable*, uint32_t> GetTable(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ConditionTabledData data_;
    KeyValueDataType kv_data_;

};

inline const ConditionTabledData& GetConditionAllTable() { return ConditionConfigurationTable::Instance().All(); }

#define FetchAndValidateConditionTable(tableId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(tableId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomConditionTable(prefix, tableId) \
const auto [##prefix##ConditionTable, prefix##fetchResult] = ConditionConfigurationTable::Instance().GetTable(tableId); \
do {if (!(##prefix##ConditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId;return (prefix##fetchResult); }} while (0)

#define FetchConditionTableOrReturnCustom(tableId, customReturnValue) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(tableId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId;return (customReturnValue); }} while (0)

#define FetchConditionTableOrReturnVoid(tableId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(tableId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId;return ;}} while (0)

#define FetchConditionTableOrContinue(tableId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(tableId); \
do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId;continue; }} while (0)

#define FetchConditionTableOrReturnFalse(tableId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(tableId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId;return false; }} while (0)