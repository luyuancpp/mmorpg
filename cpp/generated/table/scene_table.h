#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/scene_table.pb.h"

class SceneTableTempPtr  {
public:
	explicit SceneTableTempPtr(const SceneTable* ptr) : ptr_(ptr) {}

    SceneTableTempPtr(const SceneTableTempPtr&) = delete;
    SceneTableTempPtr& operator=(const SceneTableTempPtr&) = delete;
    SceneTableTempPtr(SceneTableTempPtr&&) = delete;
    SceneTableTempPtr& operator=(SceneTableTempPtr&&) = delete;

	// Support pointer-like access
	const SceneTable* operator->() const { return ptr_; }
	const SceneTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const SceneTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const SceneTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const SceneTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const SceneTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const SceneTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const SceneTable* Get() const { return ptr_; }

private:
	const SceneTable* ptr_;
};


class SceneTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SceneTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static SceneTableManager& Instance() {
        static SceneTableManager instance;
        return instance;
    }

    const SceneTabledData& All() const { return data_; }

    std::pair<SceneTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<SceneTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    SceneTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const SceneTabledData& GetSceneAllTable() {
    return SceneTableManager::Instance().All();
}

#define FetchAndValidateSceneTable(tableId) \
    const auto [sceneTable, fetchResult] = SceneTableManager::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSceneTable(prefix, tableId) \
    const auto [prefix##SceneTable, prefix##fetchResult] = SceneTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##SceneTable)) { LOG_ERROR << "Scene table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSceneTableOrReturnCustom(tableId, customReturnValue) \
    const auto [sceneTable, fetchResult] = SceneTableManager::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSceneTableOrReturnVoid(tableId) \
    const auto [sceneTable, fetchResult] = SceneTableManager::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return; } } while(0)

#define FetchSceneTableOrContinue(tableId) \
    const auto [sceneTable, fetchResult] = SceneTableManager::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; continue; } } while(0)

#define FetchSceneTableOrReturnFalse(tableId) \
    const auto [sceneTable, fetchResult] = SceneTableManager::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return false; } } while(0)