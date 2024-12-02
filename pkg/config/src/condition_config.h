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
    std::pair<const ConditionTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ConditionTabledData data_;
    KeyValueDataType kv_data_;

};

inline const ConditionTabledData& GetConditionAllTable() { return ConditionConfigurationTable::Instance().All(); }

#define FetchAndValidateConditionTable(keyId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomConditionTable(prefix, keyId) \
const auto [##prefix##ConditionTable, prefix##fetchResult] = ConditionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##ConditionTable)) { LOG_ERROR << "Condition table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchConditionTableOrReturnCustom(keyId, customReturnValue) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchConditionTableOrReturnVoid(keyId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << keyId;return ;}} while (0)

#define FetchConditionTableOrContinue(keyId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(keyId); \
do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << keyId;continue; }} while (0)

#define FetchConditionTableOrReturnFalse(keyId) \
const auto [conditionTable, fetchResult] = ConditionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << keyId;return false; }} while (0)