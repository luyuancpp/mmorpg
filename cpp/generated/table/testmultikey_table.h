#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/testmultikey_table.pb.h"

class TestMultiKeyConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestMultiKeyTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static TestMultiKeyConfigurationTable& Instance() {
        static TestMultiKeyConfigurationTable instance;
        return instance;
    }

    const TestMultiKeyTabledData& All() const { return data_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const TestMultiKeyTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    
    std::pair<const TestMultiKeyTable*, uint32_t> GetByStringkey(const std::string& tableId) const;
    const std::unordered_map<std::string, const TestMultiKeyTable*>& GetStringkeyData() const { return kv_stringkeydata_; }
        
    std::pair<const TestMultiKeyTable*, uint32_t> GetByUint32key(uint32_t tableId) const;
    const std::unordered_map<uint32_t, const TestMultiKeyTable*>& GetUint32keyData() const { return kv_uint32keydata_; }
        
    std::pair<const TestMultiKeyTable*, uint32_t> GetByIn32key(int32_t tableId) const;
    const std::unordered_map<int32_t, const TestMultiKeyTable*>& GetIn32keyData() const { return kv_in32keydata_; }
        
    std::pair<const TestMultiKeyTable*, uint32_t> GetByMstringkey(const std::string& tableId) const;
    const std::unordered_multimap<std::string, const TestMultiKeyTable*>& GetMstringkeyData() const { return kv_mstringkeydata_; }
        
    std::pair<const TestMultiKeyTable*, uint32_t> GetByMuint32key(uint32_t tableId) const;
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetMuint32keyData() const { return kv_muint32keydata_; }
        
    std::pair<const TestMultiKeyTable*, uint32_t> GetByMin32key(int32_t tableId) const;
    const std::unordered_multimap<int32_t, const TestMultiKeyTable*>& GetMin32keyData() const { return kv_min32keydata_; }
        

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    TestMultiKeyTabledData data_;
    KeyValueDataType kv_data_;
    
    std::unordered_map<std::string, const TestMultiKeyTable*> kv_stringkeydata_;
    std::unordered_map<uint32_t, const TestMultiKeyTable*> kv_uint32keydata_;
    std::unordered_map<int32_t, const TestMultiKeyTable*> kv_in32keydata_;
    std::unordered_multimap<std::string, const TestMultiKeyTable*> kv_mstringkeydata_;
    std::unordered_multimap<uint32_t, const TestMultiKeyTable*> kv_muint32keydata_;
    std::unordered_multimap<int32_t, const TestMultiKeyTable*> kv_min32keydata_;
};

inline const TestMultiKeyTabledData& GetTestMultiKeyAllTable() {
    return TestMultiKeyConfigurationTable::Instance().All();
}

#define FetchAndValidateTestMultiKeyTable(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestMultiKeyTable(prefix, tableId) \
    const auto [prefix##TestMultiKeyTable, prefix##fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##TestMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestMultiKeyTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestMultiKeyTableOrReturnVoid(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return; } } while(0)

#define FetchTestMultiKeyTableOrContinue(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestMultiKeyTableOrReturnFalse(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return false; } } while(0)