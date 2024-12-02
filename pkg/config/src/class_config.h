#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "class_config.pb.h"


class ClassConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ClassTable*>;
    static ClassConfigurationTable& Instance() { static ClassConfigurationTable instance; return instance; }
    const ClassTabledData& All() const { return data_; }
    std::pair<const ClassTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ClassTabledData data_;
    KeyValueDataType kv_data_;

};

inline const ClassTabledData& GetClassAllTable() { return ClassConfigurationTable::Instance().All(); }

#define FetchAndValidateClassTable(keyId) \
const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(keyId); \
do {if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomClassTable(prefix, keyId) \
const auto [##prefix##ClassTable, prefix##fetchResult] = ClassConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##ClassTable)) { LOG_ERROR << "Class table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchClassTableOrReturnCustom(keyId, customReturnValue) \
const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(keyId); \
do {if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchClassTableOrReturnVoid(keyId) \
const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(keyId); \
do {if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << keyId;return ;}} while (0)

#define FetchClassTableOrContinue(keyId) \
const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(keyId); \
do { if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << keyId;continue; }} while (0)

#define FetchClassTableOrReturnFalse(keyId) \
const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(keyId); \
do {if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << keyId;return false; }} while (0)