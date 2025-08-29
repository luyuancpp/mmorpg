#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/reward_table.pb.h"

class RewardTableTempPtr  {
public:
	explicit RewardTableTempPtr(const RewardTable* ptr) : ptr_(ptr) {}

	// Support pointer-like access
	const RewardTable* operator->() const { return ptr_; }
	const RewardTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const RewardTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const RewardTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const RewardTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const RewardTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const RewardTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const RewardTable* Get() const { return ptr_; }

private:
	const RewardTable* ptr_;
};


class RewardTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const RewardTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static RewardTableManager& Instance() {
        static RewardTableManager instance;
        return instance;
    }

    const RewardTabledData& All() const { return data_; }

    std::pair<const RewardTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<const RewardTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    RewardTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const RewardTabledData& GetRewardAllTable() {
    return RewardTableManager::Instance().All();
}

#define FetchAndValidateRewardTable(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().GetTable(tableId); \
    do { if (!( rewardTable )) { LOG_ERROR << "Reward table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomRewardTable(prefix, tableId) \
    const auto [prefix##RewardTable, prefix##fetchResult] = RewardTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##RewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchRewardTableOrReturnCustom(tableId, customReturnValue) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().GetTable(tableId); \
    do { if (!( rewardTable )) { LOG_ERROR << "Reward table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchRewardTableOrReturnVoid(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().GetTable(tableId); \
    do { if (!( rewardTable )) { LOG_ERROR << "Reward table not found for ID: " << tableId; return; } } while(0)

#define FetchRewardTableOrContinue(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().GetTable(tableId); \
    do { if (!( rewardTable )) { LOG_ERROR << "Reward table not found for ID: " << tableId; continue; } } while(0)

#define FetchRewardTableOrReturnFalse(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().GetTable(tableId); \
    do { if (!( rewardTable )) { LOG_ERROR << "Reward table not found for ID: " << tableId; return false; } } while(0)