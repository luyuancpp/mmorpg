#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/cooldown_table.pb.h"

class CooldownTableTempPtr  {
public:
	explicit CooldownTableTempPtr(const CooldownTable* ptr) : ptr_(ptr) {}

    CooldownTableTempPtr(const CooldownTableTempPtr&) = delete;
    CooldownTableTempPtr& operator=(const CooldownTableTempPtr&) = delete;
    CooldownTableTempPtr(CooldownTableTempPtr&&) = delete;
    CooldownTableTempPtr& operator=(CooldownTableTempPtr&&) = delete;

	// Support pointer-like access
	const CooldownTable* operator->() const { return ptr_; }
	const CooldownTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const CooldownTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const CooldownTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const CooldownTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const CooldownTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const CooldownTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const CooldownTable* Get() const { return ptr_; }

private:
	const CooldownTable* ptr_;
};


class CooldownTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const CooldownTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static CooldownTableManager& Instance() {
        static CooldownTableManager instance;
        return instance;
    }

    const CooldownTabledData& All() const { return data_; }

    std::pair<CooldownTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<CooldownTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    CooldownTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const CooldownTabledData& GetCooldownAllTable() {
    return CooldownTableManager::Instance().All();
}

#define FetchAndValidateCooldownTable(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomCooldownTable(prefix, tableId) \
    const auto [prefix##CooldownTable, prefix##fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##CooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchCooldownTableOrReturnCustom(tableId, customReturnValue) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchCooldownTableOrReturnVoid(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return; } } while(0)

#define FetchCooldownTableOrContinue(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; continue; } } while(0)

#define FetchCooldownTableOrReturnFalse(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return false; } } while(0)