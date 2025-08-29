#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/globalvariable_table.pb.h"

class GlobalVariableTableTempPtr  {
public:
	explicit GlobalVariableTableTempPtr(const GlobalVariableTable* ptr) : ptr_(ptr) {}

	// Support pointer-like access
	const GlobalVariableTable* operator->() const { return ptr_; }
	const GlobalVariableTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const GlobalVariableTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const GlobalVariableTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const GlobalVariableTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const GlobalVariableTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const GlobalVariableTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const GlobalVariableTable* Get() const { return ptr_; }

private:
	const GlobalVariableTable* ptr_;
};


class GlobalVariableTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static GlobalVariableTableManager& Instance() {
        static GlobalVariableTableManager instance;
        return instance;
    }

    const GlobalVariableTabledData& All() const { return data_; }

    std::pair<const GlobalVariableTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<const GlobalVariableTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    GlobalVariableTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const GlobalVariableTabledData& GetGlobalVariableAllTable() {
    return GlobalVariableTableManager::Instance().All();
}

#define FetchAndValidateGlobalVariableTable(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomGlobalVariableTable(prefix, tableId) \
    const auto [prefix##GlobalVariableTable, prefix##fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##GlobalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchGlobalVariableTableOrReturnCustom(tableId, customReturnValue) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchGlobalVariableTableOrReturnVoid(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return; } } while(0)

#define FetchGlobalVariableTableOrContinue(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; continue; } } while(0)

#define FetchGlobalVariableTableOrReturnFalse(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!( globalVariableTable )) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return false; } } while(0)