#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "test_config.pb.h"


class TestConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const TestTable*>;
    static TestConfigurationTable& GetSingleton() { static TestConfigurationTable singleton; return singleton; }
    const TestTabledData& All() const { return data_; }
    std::pair<const TestTable*, uint32_t> GetTable(uint32_t keyId);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();


private:
    TestTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const TestTable*, uint32_t> GetTestTable(const uint32_t keyId) { return TestConfigurationTable::GetSingleton().GetTable(keyId); }

inline const TestTabledData& GetTestAllTable() { return TestConfigurationTable::GetSingleton().All(); }