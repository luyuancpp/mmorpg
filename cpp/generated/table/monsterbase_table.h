#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/monsterbase_table.pb.h"

class MonsterBaseTableTempPtr  {
public:
	explicit MonsterBaseTableTempPtr(const MonsterBaseTable* ptr) : ptr_(ptr) {}

    MonsterBaseTableTempPtr(const MonsterBaseTableTempPtr&) = delete;
    MonsterBaseTableTempPtr& operator=(const MonsterBaseTableTempPtr&) = delete;
    MonsterBaseTableTempPtr(MonsterBaseTableTempPtr&&) = delete;
    MonsterBaseTableTempPtr& operator=(MonsterBaseTableTempPtr&&) = delete;

	// Support pointer-like access
	const MonsterBaseTable* operator->() const { return ptr_; }
	const MonsterBaseTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const MonsterBaseTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const MonsterBaseTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const MonsterBaseTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const MonsterBaseTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const MonsterBaseTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const MonsterBaseTable* Get() const { return ptr_; }

private:
	const MonsterBaseTable* ptr_;
};


class MonsterBaseTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MonsterBaseTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static MonsterBaseTableManager& Instance() {
        static MonsterBaseTableManager instance;
        return instance;
    }

    const MonsterBaseTabledData& All() const { return data_; }

    std::pair<MonsterBaseTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<MonsterBaseTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    MonsterBaseTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const MonsterBaseTabledData& GetMonsterBaseAllTable() {
    return MonsterBaseTableManager::Instance().All();
}

#define FetchAndValidateMonsterBaseTable(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseTableManager::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMonsterBaseTable(prefix, tableId) \
    const auto [prefix##MonsterBaseTable, prefix##fetchResult] = MonsterBaseTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MonsterBaseTable)) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMonsterBaseTableOrReturnCustom(tableId, customReturnValue) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseTableManager::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMonsterBaseTableOrReturnVoid(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseTableManager::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return; } } while(0)

#define FetchMonsterBaseTableOrContinue(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseTableManager::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; continue; } } while(0)

#define FetchMonsterBaseTableOrReturnFalse(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseTableManager::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return false; } } while(0)