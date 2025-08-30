#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/mission_table.pb.h"

class MissionTableTempPtr  {
public:
	explicit MissionTableTempPtr(const MissionTable* ptr) : ptr_(ptr) {}

    MissionTableTempPtr(const MissionTableTempPtr&) = delete;
    MissionTableTempPtr& operator=(const MissionTableTempPtr&) = delete;
    MissionTableTempPtr(MissionTableTempPtr&&) = delete;
    MissionTableTempPtr& operator=(MissionTableTempPtr&&) = delete;

	// Support pointer-like access
	const MissionTable* operator->() const { return ptr_; }
	const MissionTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const MissionTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const MissionTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const MissionTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const MissionTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const MissionTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const MissionTable* Get() const { return ptr_; }

private:
	const MissionTable* ptr_;
};


class MissionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MissionTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static MissionTableManager& Instance() {
        static MissionTableManager instance;
        return instance;
    }

    const MissionTabledData& All() const { return data_; }

    std::pair<MissionTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<MissionTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    MissionTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const MissionTabledData& GetMissionAllTable() {
    return MissionTableManager::Instance().All();
}

#define FetchAndValidateMissionTable(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMissionTable(prefix, tableId) \
    const auto [prefix##MissionTable, prefix##fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MissionTable)) { LOG_ERROR << "Mission table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMissionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMissionTableOrReturnVoid(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return; } } while(0)

#define FetchMissionTableOrContinue(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; continue; } } while(0)

#define FetchMissionTableOrReturnFalse(tableId) \
    const auto [missionTable, fetchResult] = MissionTableManager::Instance().GetTable(tableId); \
    do { if (!( missionTable )) { LOG_ERROR << "Mission table not found for ID: " << tableId; return false; } } while(0)