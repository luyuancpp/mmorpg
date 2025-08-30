#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/testmultikey_table.pb.h"

class TestMultiKeyTableTempPtr  {
public:
	explicit TestMultiKeyTableTempPtr(const TestMultiKeyTable* ptr) : ptr_(ptr) {}

    TestMultiKeyTableTempPtr(const TestMultiKeyTableTempPtr&) = delete;
    TestMultiKeyTableTempPtr& operator=(const TestMultiKeyTableTempPtr&) = delete;
    TestMultiKeyTableTempPtr(TestMultiKeyTableTempPtr&&) = delete;
    TestMultiKeyTableTempPtr& operator=(TestMultiKeyTableTempPtr&&) = delete;

	// Support pointer-like access
	const TestMultiKeyTable* operator->() const { return ptr_; }
	const TestMultiKeyTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const TestMultiKeyTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const TestMultiKeyTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const TestMultiKeyTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const TestMultiKeyTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const TestMultiKeyTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const TestMultiKeyTable* Get() const { return ptr_; }

private:
	const TestMultiKeyTable* ptr_;
};


class TestMultiKeyTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestMultiKeyTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static TestMultiKeyTableManager& Instance() {
        static TestMultiKeyTableManager instance;
        return instance;
    }

    const TestMultiKeyTabledData& All() const { return data_; }

    std::pair<TestMultiKeyTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<TestMultiKeyTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    
    std::pair< TestMultiKeyTableTempPtr, uint32_t> GetByStringkey(const std::string& tableId) const;
    const std::unordered_map<std::string, const TestMultiKeyTableTempPtr>& GetStringkeyData() const { return kv_stringkeydata_; }
        
    std::pair< TestMultiKeyTableTempPtr, uint32_t> GetByUint32key(uint32_t tableId) const;
    const std::unordered_map<uint32_t, const TestMultiKeyTableTempPtr>& GetUint32keyData() const { return kv_uint32keydata_; }
        
    std::pair< TestMultiKeyTableTempPtr, uint32_t> GetByIn32key(int32_t tableId) const;
    const std::unordered_map<int32_t, const TestMultiKeyTableTempPtr>& GetIn32keyData() const { return kv_in32keydata_; }
        
    std::pair< TestMultiKeyTableTempPtr, uint32_t> GetByMstringkey(const std::string& tableId) const;
    const std::unordered_multimap<std::string, const TestMultiKeyTableTempPtr>& GetMstringkeyData() const { return kv_mstringkeydata_; }
        
    std::pair< TestMultiKeyTableTempPtr, uint32_t> GetByMuint32key(uint32_t tableId) const;
    const std::unordered_multimap<uint32_t, const TestMultiKeyTableTempPtr>& GetMuint32keyData() const { return kv_muint32keydata_; }
        
    std::pair< TestMultiKeyTableTempPtr, uint32_t> GetByMin32key(int32_t tableId) const;
    const std::unordered_multimap<int32_t, const TestMultiKeyTableTempPtr>& GetMin32keyData() const { return kv_min32keydata_; }
        

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    TestMultiKeyTabledData data_;
    KeyValueDataType kv_data_;
    
    std::unordered_map<std::string, const TestMultiKeyTableTempPtr> kv_stringkeydata_;
    std::unordered_map<uint32_t, const TestMultiKeyTableTempPtr> kv_uint32keydata_;
    std::unordered_map<int32_t, const TestMultiKeyTableTempPtr> kv_in32keydata_;
    std::unordered_multimap<std::string, const TestMultiKeyTableTempPtr> kv_mstringkeydata_;
    std::unordered_multimap<uint32_t, const TestMultiKeyTableTempPtr> kv_muint32keydata_;
    std::unordered_multimap<int32_t, const TestMultiKeyTableTempPtr> kv_min32keydata_;
};

inline const TestMultiKeyTabledData& GetTestMultiKeyAllTable() {
    return TestMultiKeyTableManager::Instance().All();
}

#define FetchAndValidateTestMultiKeyTable(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestMultiKeyTable(prefix, tableId) \
    const auto [prefix##TestMultiKeyTable, prefix##fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##TestMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestMultiKeyTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestMultiKeyTableOrReturnVoid(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return; } } while(0)

#define FetchTestMultiKeyTableOrContinue(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestMultiKeyTableOrReturnFalse(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().GetTable(tableId); \
    do { if (!( testMultiKeyTable )) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return false; } } while(0)