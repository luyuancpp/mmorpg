#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
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

inline std::pair<const TestTable*, uint32_t> GetTestTable(const uint32_t keyId) { return TestConfigurationTable::Instance().GetTable(keyId); }

inline const TestTabledData& GetTestAllTable() { return TestConfigurationTable::Instance().All(); }