#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "test_config.pb.h"

class TestConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestTable*>;

    static TestConfigurationTable& Instance() {
        static TestConfigurationTable instance;
        return instance;
    }

    const TestTabledData& All() const { return data_; }
    std::pair<const TestTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const TestTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    TestTabledData data_;
    KeyValueDataType kv_data_;
};

inline const TestTabledData& GetTestAllTable() {
    return TestConfigurationTable::Instance().All();
}

#define FetchAndValidateTestTable(tableId) \
    const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestTable(prefix, tableId) \
    const auto [prefix##TestTable, prefix##fetchResult] = TestConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##TestTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestTableOrReturnVoid(tableId) \
    const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return; } } while(0)

#define FetchTestTableOrContinue(tableId) \
    const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestTableOrReturnFalse(tableId) \
    const auto [testTable, fetchResult] = TestConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return false; } } while(0)