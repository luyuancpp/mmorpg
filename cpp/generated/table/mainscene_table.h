#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/mainscene_table.pb.h"

class MainSceneTableTempPtr  {
public:
	explicit MainSceneTableTempPtr(const MainSceneTable* ptr) : ptr_(ptr) {}

    MainSceneTableTempPtr(const MainSceneTableTempPtr&) = delete;
    MainSceneTableTempPtr& operator=(const MainSceneTableTempPtr&) = delete;
    MainSceneTableTempPtr(MainSceneTableTempPtr&&) = delete;
    MainSceneTableTempPtr& operator=(MainSceneTableTempPtr&&) = delete;

	// Support pointer-like access
	const MainSceneTable* operator->() const { return ptr_; }
	const MainSceneTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const MainSceneTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const MainSceneTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const MainSceneTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const MainSceneTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const MainSceneTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const MainSceneTable* Get() const { return ptr_; }

private:
	const MainSceneTable* ptr_;
};


class MainSceneTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MainSceneTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static MainSceneTableManager& Instance() {
        static MainSceneTableManager instance;
        return instance;
    }

    const MainSceneTabledData& All() const { return data_; }

    std::pair<MainSceneTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<MainSceneTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    MainSceneTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const MainSceneTabledData& GetMainSceneAllTable() {
    return MainSceneTableManager::Instance().All();
}

#define FetchAndValidateMainSceneTable(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMainSceneTable(prefix, tableId) \
    const auto [prefix##MainSceneTable, prefix##fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMainSceneTableOrReturnCustom(tableId, customReturnValue) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMainSceneTableOrReturnVoid(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return; } } while(0)

#define FetchMainSceneTableOrContinue(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; continue; } } while(0)

#define FetchMainSceneTableOrReturnFalse(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return false; } } while(0)