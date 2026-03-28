
#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/item_table.pb.h"

class ItemTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static ItemTableManager& Instance() {
        static ItemTableManager instance;
        return instance;
    }

    const ItemTableData& All() const { return data_; }

    std::pair<const ItemTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ItemTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }







private:
    LoadSuccessCallback loadSuccessCallback_;
    ItemTableData data_;
    KeyValueDataType kv_data_;


};

inline const ItemTableData& GetItemAllTable() {
    return ItemTableManager::Instance().All();
}

#define FetchAndValidateItemTable(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomItemTable(prefix, tableId) \
    const auto [prefix##ItemTable, prefix##fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ItemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchItemTableOrReturnCustom(tableId, customReturnValue) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchItemTableOrReturnVoid(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return; } } while(0)

#define FetchItemTableOrContinue(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; continue; } } while(0)

#define FetchItemTableOrReturnFalse(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().GetTable(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return false; } } while(0)