#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/item_table.pb.h"

class ItemTableTempPtr  {
public:
	explicit ItemTableTempPtr(const ItemTable* ptr) : ptr_(ptr) {}

    ItemTableTempPtr(const ItemTableTempPtr&) = delete;
    ItemTableTempPtr& operator=(const ItemTableTempPtr&) = delete;
    ItemTableTempPtr(ItemTableTempPtr&&) = delete;
    ItemTableTempPtr& operator=(ItemTableTempPtr&&) = delete;

	// Support pointer-like access
	const ItemTable* operator->() const { return ptr_; }
	const ItemTable& operator*()  const { return *ptr_; }

	// Enable usage in boolean expressions
	explicit operator bool() const { return ptr_ != nullptr; }

	// Enable comparison with nullptr (does NOT trigger deprecation)
	friend bool operator==(const ItemTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const ItemTableTempPtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const ItemTableTempPtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const ItemTableTempPtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 Dangerous: implicit conversion to raw pointer (triggers warning)
	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	operator const ItemTable* () const { return ptr_; }

	[[deprecated("Do not store this pointer. It's only valid temporarily and may cause crashes after hot-reloading.")]]
	const ItemTable* Get() const { return ptr_; }

private:
	const ItemTable* ptr_;
};


class ItemTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ItemTableManager& Instance() {
        static ItemTableManager instance;
        return instance;
    }

    const ItemTabledData& All() const { return data_; }

    std::pair<ItemTableTempPtr, uint32_t> GetTable(uint32_t tableId);
    std::pair<ItemTableTempPtr, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ItemTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ItemTabledData& GetItemAllTable() {
    return ItemTableManager::Instance().All();
}

#define FetchAndValidateItemTable(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomItemTable(prefix, tableId) \
    const auto [prefix##ItemTable, prefix##fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ItemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchItemTableOrReturnCustom(tableId, customReturnValue) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchItemTableOrReturnVoid(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return; } } while(0)

#define FetchItemTableOrContinue(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; continue; } } while(0)

#define FetchItemTableOrReturnFalse(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return false; } } while(0)