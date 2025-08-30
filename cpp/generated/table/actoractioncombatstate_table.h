#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/actoractioncombatstate_table.pb.h"

class ActorActionCombatStateTableTempPtr  {
public:
	explicit ActorActionCombatStateTableTempPtr(const ActorActionCombatStateTable* ptr) : ptr_(ptr) {}

    ActorActionCombatStateTableTempPtr(const ActorActionCombatStateTableTempPtr&) = delete;
    ActorActionCombatStateTableTempPtr& operator=(const ActorActionCombatStateTableTempPtr&) = delete;
    ActorActionCombatStateTableTempPtr(ActorActionCombatStateTableTempPtr&&) = delete;
    ActorActionCombatStateTableTempPtr& operator=(ActorActionCombatStateTableTempPtr&&) = delete;

	// Support pointer-like access
	const ActorActionCombatStateTable* operator->() const { return ptr_; }
	const ActorActionCombatStateTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const ActorActionCombatStateTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const ActorActionCombatStateTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const ActorActionCombatStateTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const ActorActionCombatStateTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const ActorActionCombatStateTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const ActorActionCombatStateTable* Get() const { return ptr_; }

private:
	const ActorActionCombatStateTable* ptr_;
};


class ActorActionCombatStateTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionCombatStateTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ActorActionCombatStateTableManager& Instance() {
        static ActorActionCombatStateTableManager instance;
        return instance;
    }

    const ActorActionCombatStateTabledData& All() const { return data_; }

    std::pair<ActorActionCombatStateTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<ActorActionCombatStateTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ActorActionCombatStateTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ActorActionCombatStateTabledData& GetActorActionCombatStateAllTable() {
    return ActorActionCombatStateTableManager::Instance().All();
}

#define FetchAndValidateActorActionCombatStateTable(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionCombatStateTable(prefix, tableId) \
    const auto [prefix##ActorActionCombatStateTable, prefix##fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionCombatStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnVoid(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionCombatStateTableOrContinue(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnFalse(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return false; } } while(0)