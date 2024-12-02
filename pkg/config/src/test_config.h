#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "test_config.pb.h"


class TestConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestTable*>;
    static TestConfigurationTable& Instance() { static TestConfigurationTable instance; return instance; }
    const TestTabledData& All() const { return data_; }
    std::pair<const TestTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    TestTabledData data_;
    KeyValueDataType kv_data_;

};

inline const TestTabledData& GetTestAllTable() { return TestConfigurationTable::Instance().All(); }

#define FetchAndValidateTestTable(keyId) \
const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomTestTable(prefix, keyId) \
const auto [##prefix##TestTable, prefix##fetchResult] = TestConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##TestTable)) { LOG_ERROR << "Test table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchTestTableOrReturnCustom(keyId, customReturnValue) \
const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchTestTableOrReturnVoid(keyId) \
const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << keyId;return ;}} while (0)

#define FetchTestTableOrContinue(keyId) \
const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(keyId); \
do { if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << keyId;continue; }} while (0)

#define FetchTestTableOrReturnFalse(keyId) \
const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << keyId;return false; }} while (0)