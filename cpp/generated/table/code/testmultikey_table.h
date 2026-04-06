#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/testmultikey_table.pb.h"

class TestMultiKeyTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestMultiKeyTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static TestMultiKeyTableManager& Instance() {
        static TestMultiKeyTableManager instance;
        return instance;
    }

    const TestMultiKeyTableData& All() const { return data_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const TestMultiKeyTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    std::pair<const TestMultiKeyTable*, uint32_t> GetByString_key(const std::string& tableId) const;
    const std::unordered_map<std::string, const TestMultiKeyTable*>& GetString_keyData() const { return kv_string_keydata_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetByUint32_key(uint32_t tableId) const;
    const std::unordered_map<uint32_t, const TestMultiKeyTable*>& GetUint32_keyData() const { return kv_uint32_keydata_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetByInt32_key(int32_t tableId) const;
    const std::unordered_map<int32_t, const TestMultiKeyTable*>& GetInt32_keyData() const { return kv_int32_keydata_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetByM_string_key(const std::string& tableId) const;
    const std::unordered_multimap<std::string, const TestMultiKeyTable*>& GetM_string_keyData() const { return kv_m_string_keydata_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetByM_uint32_key(uint32_t tableId) const;
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetM_uint32_keyData() const { return kv_m_uint32_keydata_; }

    std::pair<const TestMultiKeyTable*, uint32_t> GetByM_int32_key(int32_t tableId) const;
    const std::unordered_multimap<int32_t, const TestMultiKeyTable*>& GetM_int32_keyData() const { return kv_m_int32_keydata_; }

    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetEffectIndex() const { return idx_effect_; }

private:
    LoadSuccessCallback loadSuccessCallback_;
    TestMultiKeyTableData data_;
    KeyValueDataType kv_data_;
    std::unordered_map<std::string, const TestMultiKeyTable*> kv_string_keydata_;
    std::unordered_map<uint32_t, const TestMultiKeyTable*> kv_uint32_keydata_;
    std::unordered_map<int32_t, const TestMultiKeyTable*> kv_int32_keydata_;
    std::unordered_multimap<std::string, const TestMultiKeyTable*> kv_m_string_keydata_;
    std::unordered_multimap<uint32_t, const TestMultiKeyTable*> kv_m_uint32_keydata_;
    std::unordered_multimap<int32_t, const TestMultiKeyTable*> kv_m_int32_keydata_;
    std::unordered_multimap<uint32_t, const TestMultiKeyTable*> idx_effect_;
};

inline const TestMultiKeyTableData& GetTestMultiKeyAllTable() {
    return TestMultiKeyTableManager::Instance().All();
}

#define FetchAndValidateTestMultiKeyTable(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestMultiKeyTable(prefix, tableId) \
    const auto [prefix##TestMultiKeyTable, prefix##fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##TestMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestMultiKeyTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestMultiKeyTableOrReturnVoid(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return; } } while(0)

#define FetchTestMultiKeyTableOrContinue(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestMultiKeyTableOrReturnFalse(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return false; } } while(0)
