#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/condition_table.pb.h"

class ConditionTableTempPtr  {
public:
	explicit ConditionTableTempPtr(const ConditionTable* ptr) : ptr_(ptr) {}

    ConditionTableTempPtr(const ConditionTableTempPtr&) = delete;
    ConditionTableTempPtr& operator=(const ConditionTableTempPtr&) = delete;
    ConditionTableTempPtr(ConditionTableTempPtr&&) = delete;
    ConditionTableTempPtr& operator=(ConditionTableTempPtr&&) = delete;

	// Support pointer-like access
	const ConditionTable* operator->() const { return ptr_; }
	const ConditionTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const ConditionTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const ConditionTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const ConditionTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const ConditionTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const ConditionTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const ConditionTable* Get() const { return ptr_; }

private:
	const ConditionTable* ptr_;
};


class ConditionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ConditionTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ConditionTableManager& Instance() {
        static ConditionTableManager instance;
        return instance;
    }

    const ConditionTabledData& All() const { return data_; }

    std::pair<ConditionTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<ConditionTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ConditionTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ConditionTabledData& GetConditionAllTable() {
    return ConditionTableManager::Instance().All();
}

#define FetchAndValidateConditionTable(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomConditionTable(prefix, tableId) \
    const auto [prefix##ConditionTable, prefix##fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ConditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchConditionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchConditionTableOrReturnVoid(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return; } } while(0)

#define FetchConditionTableOrContinue(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; continue; } } while(0)

#define FetchConditionTableOrReturnFalse(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().GetTable(tableId); \
    do { if (!( conditionTable )) { LOG_ERROR << "Condition table not found for ID: " << tableId; return false; } } while(0)