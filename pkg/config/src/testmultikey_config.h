#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "testmultikey_config.pb.h"


class TestMultiKeyConfigurationTable {
public:
    using KeyValueDataType = std::unordered_multimap<uint32_t, const TestMultiKeyTable*>;
    static TestMultiKeyConfigurationTable& Instance() { static TestMultiKeyConfigurationTable instance; return instance; }
    const TestMultiKeyTabledData& All() const { return data_; }
    std::pair<const TestMultiKeyTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();

    std::pair<const TestMultiKeyTable*, uint32_t> GetByStringkey(const std::string& keyId) const;
    const std::unordered_map<std::string, const TestMultiKeyTable*>& GetStringkeyData() const { return kv_stringkeydata_; }
    std::pair<const TestMultiKeyTable*, uint32_t> GetByUint32Key(uint32_t keyId) const;
    const std::unordered_map<uint32_t, const TestMultiKeyTable*>& GetUint32KeyData() const { return kv_uint32keydata_; }
    std::pair<const TestMultiKeyTable*, uint32_t> GetByIn32Key(int32_t keyId) const;
    const std::unordered_map<int32_t, const TestMultiKeyTable*>& GetIn32KeyData() const { return kv_in32keydata_; }
    std::pair<const TestMultiKeyTable*, uint32_t> GetByMstringkey(const std::string& keyId) const;
    const std::unordered_multimap<std::string, const TestMultiKeyTable*>& GetMstringkeyData() const { return kv_mstringkeydata_; }
    std::pair<const TestMultiKeyTable*, uint32_t> GetByMuint32Key(uint32_t keyId) const;
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetMuint32KeyData() const { return kv_muint32keydata_; }
    std::pair<const TestMultiKeyTable*, uint32_t> GetByMin32Key(int32_t keyId) const;
    const std::unordered_multimap<int32_t, const TestMultiKeyTable*>& GetMin32KeyData() const { return kv_min32keydata_; }

private:
    TestMultiKeyTabledData data_;
    KeyValueDataType kv_data_;

    std::unordered_map<std::string, const TestMultiKeyTable*>  kv_stringkeydata_;
    std::unordered_map<uint32_t, const TestMultiKeyTable*>  kv_uint32keydata_;
    std::unordered_map<int32_t, const TestMultiKeyTable*>  kv_in32keydata_;
    std::unordered_multimap<std::string, const TestMultiKeyTable*>  kv_mstringkeydata_;
    std::unordered_multimap<uint32_t, const TestMultiKeyTable*>  kv_muint32keydata_;
    std::unordered_multimap<int32_t, const TestMultiKeyTable*>  kv_min32keydata_;
};

inline const TestMultiKeyTabledData& GetTestMultiKeyAllTable() { return TestMultiKeyConfigurationTable::Instance().All(); }

#define FetchAndValidateTestMultiKeyTable(keyId) \
const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomTestMultiKeyTable(prefix, keyId) \
const auto [##prefix##TestMultiKeyTable, prefix##fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##TestMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchTestMultiKeyTableOrReturnCustom(keyId, customReturnValue) \
const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchTestMultiKeyTableOrReturnVoid(keyId) \
const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << keyId;return ;}} while (0)

#define FetchTestMultiKeyTableOrContinue(keyId) \
const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(keyId); \
do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << keyId;continue; }} while (0)

#define FetchTestMultiKeyTableOrReturnFalse(keyId) \
const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(keyId); \
do {if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << keyId;return false; }} while (0)