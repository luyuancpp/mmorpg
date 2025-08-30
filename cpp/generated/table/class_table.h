#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/class_table.pb.h"

class ClassTableTempPtr  {
public:
	explicit ClassTableTempPtr(const ClassTable* ptr) : ptr_(ptr) {}

    ClassTableTempPtr(const ClassTableTempPtr&) = delete;
    ClassTableTempPtr& operator=(const ClassTableTempPtr&) = delete;
    ClassTableTempPtr(ClassTableTempPtr&&) = delete;
    ClassTableTempPtr& operator=(ClassTableTempPtr&&) = delete;

	// Support pointer-like access
	const ClassTable* operator->() const { return ptr_; }
	const ClassTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const ClassTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const ClassTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const ClassTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const ClassTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const ClassTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const ClassTable* Get() const { return ptr_; }

private:
	const ClassTable* ptr_;
};


class ClassTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ClassTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ClassTableManager& Instance() {
        static ClassTableManager instance;
        return instance;
    }

    const ClassTabledData& All() const { return data_; }

    std::pair<ClassTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<ClassTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ClassTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ClassTabledData& GetClassAllTable() {
    return ClassTableManager::Instance().All();
}

#define FetchAndValidateClassTable(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomClassTable(prefix, tableId) \
    const auto [prefix##ClassTable, prefix##fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ClassTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchClassTableOrReturnCustom(tableId, customReturnValue) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchClassTableOrReturnVoid(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return; } } while(0)

#define FetchClassTableOrContinue(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; continue; } } while(0)

#define FetchClassTableOrReturnFalse(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return false; } } while(0)