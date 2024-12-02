#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "globalvariable_config.pb.h"


class GlobalVariableConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;
    static GlobalVariableConfigurationTable& Instance() { static GlobalVariableConfigurationTable instance; return instance; }
    const GlobalVariableTabledData& All() const { return data_; }
    std::pair<const GlobalVariableTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    GlobalVariableTabledData data_;
    KeyValueDataType kv_data_;

};

inline const GlobalVariableTabledData& GetGlobalVariableAllTable() { return GlobalVariableConfigurationTable::Instance().All(); }

#define FetchAndValidateGlobalVariableTable(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomGlobalVariableTable(prefix, keyId) \
const auto [##prefix##GlobalVariableTable, prefix##fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##GlobalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchGlobalVariableTableOrReturnCustom(keyId, customReturnValue) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchGlobalVariableTableOrReturnVoid(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;return ;}} while (0)

#define FetchGlobalVariableTableOrContinue(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;continue; }} while (0)

#define FetchGlobalVariableTableOrReturnFalse(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;return false; }} while (0)