#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/test_table.pb.h"

class TestTableTempPtr  {
public:
	explicit TestTableTempPtr(const TestTable* ptr) : ptr_(ptr) {}

    TestTableTempPtr(const TestTableTempPtr&) = delete;
    TestTableTempPtr& operator=(const TestTableTempPtr&) = delete;
    TestTableTempPtr(TestTableTempPtr&&) = delete;
    TestTableTempPtr& operator=(TestTableTempPtr&&) = delete;

	// Support pointer-like access
	const TestTable* operator->() const { return ptr_; }
	const TestTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const TestTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const TestTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const TestTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const TestTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const TestTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const TestTable* Get() const { return ptr_; }

private:
	const TestTable* ptr_;
};


class TestTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static TestTableManager& Instance() {
        static TestTableManager instance;
        return instance;
    }

    const TestTabledData& All() const { return data_; }

    std::pair<TestTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<TestTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    TestTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const TestTabledData& GetTestAllTable() {
    return TestTableManager::Instance().All();
}

#define FetchAndValidateTestTable(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestTable(prefix, tableId) \
    const auto [prefix##TestTable, prefix##fetchResult] = TestTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##TestTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestTableOrReturnVoid(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return; } } while(0)

#define FetchTestTableOrContinue(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestTableOrReturnFalse(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().GetTable(tableId); \
    do { if (!( testTable )) { LOG_ERROR << "Test table not found for ID: " << tableId; return false; } } while(0)