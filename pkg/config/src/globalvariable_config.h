#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
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

inline std::pair<const GlobalVariableTable*, uint32_t> GetGlobalVariableTable(const uint32_t keyId) { return GlobalVariableConfigurationTable::Instance().GetTable(keyId); }

inline const GlobalVariableTabledData& GetGlobalVariableAllTable() { return GlobalVariableConfigurationTable::Instance().All(); }

#define FetchAndValidateGlobalVariableTable(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
if (!(globalVariableTable)) { return (fetchResult); }

#define FetchGlobalVariableTableOrReturnVoid(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(globalVariableTable)) { return ;}} while (0)

#define FetchGlobalVariableTableOrContinue(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do { if (!(globalVariableTable)) { continue; }} while (0)

#define FetchGlobalVariableTableOrReturnFalse(keyId) \
const auto [globalVariableTable, fetchResult] = GlobalVariableConfigurationTable::Instance().GetTable(keyId); \
do {if (!(globalVariableTable)) { return false; }} while (0)