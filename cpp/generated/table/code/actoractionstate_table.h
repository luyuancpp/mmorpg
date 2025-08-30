#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/actoractionstate_table.pb.h"

class ActorActionStateTableTempPtr  {
public:
	explicit ActorActionStateTableTempPtr(const ActorActionStateTable* ptr) : ptr_(ptr) {}

    ActorActionStateTableTempPtr(const ActorActionStateTableTempPtr&) = delete;
    ActorActionStateTableTempPtr& operator=(const ActorActionStateTableTempPtr&) = delete;
    ActorActionStateTableTempPtr(ActorActionStateTableTempPtr&&) = delete;
    ActorActionStateTableTempPtr& operator=(ActorActionStateTableTempPtr&&) = delete;

	// Support pointer-like access
	const ActorActionStateTable* operator->() const { return ptr_; }
	const ActorActionStateTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const ActorActionStateTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const ActorActionStateTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const ActorActionStateTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const ActorActionStateTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const ActorActionStateTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const ActorActionStateTable* Get() const { return ptr_; }

private:
	const ActorActionStateTable* ptr_;
};


class ActorActionStateTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionStateTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ActorActionStateTableManager& Instance() {
        static ActorActionStateTableManager instance;
        return instance;
    }

    const ActorActionStateTabledData& All() const { return data_; }

    std::pair<ActorActionStateTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<ActorActionStateTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ActorActionStateTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ActorActionStateTabledData& GetActorActionStateAllTable() {
    return ActorActionStateTableManager::Instance().All();
}

#define FetchAndValidateActorActionStateTable(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionStateTable(prefix, tableId) \
    const auto [prefix##ActorActionStateTable, prefix##fetchResult] = ActorActionStateTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionStateTableOrReturnVoid(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionStateTableOrContinue(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionStateTableOrReturnFalse(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return false; } } while(0)