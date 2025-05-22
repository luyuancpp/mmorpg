#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "item_config.pb.h"

class ItemConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;

    static ItemConfigurationTable& Instance() {
        static ItemConfigurationTable instance;
        return instance;
    }

    const ItemTabledData& All() const { return data_; }
    std::pair<const ItemTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ItemTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    ItemTabledData data_;
    KeyValueDataType kv_data_;
};

inline const ItemTabledData& GetItemAllTable() {
    return ItemConfigurationTable::Instance().All();
}

#define FetchAndValidateItemTable(tableId) \
    const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomItemTable(prefix, tableId) \
    const auto [prefix##ItemTable, prefix##fetchResult] = ItemConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##ItemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchItemTableOrReturnCustom(tableId, customReturnValue) \
    const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchItemTableOrReturnVoid(tableId) \
    const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return; } } while(0)

#define FetchItemTableOrContinue(tableId) \
    const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; continue; } } while(0)

#define FetchItemTableOrReturnFalse(tableId) \
    const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( itemTable )) { LOG_ERROR << "Item table not found for ID: " << tableId; return false; } } while(0)