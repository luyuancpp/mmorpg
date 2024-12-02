#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "item_config.pb.h"


class ItemConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;
    static ItemConfigurationTable& Instance() { static ItemConfigurationTable instance; return instance; }
    const ItemTabledData& All() const { return data_; }
    std::pair<const ItemTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ItemTabledData data_;
    KeyValueDataType kv_data_;

};

inline const ItemTabledData& GetItemAllTable() { return ItemConfigurationTable::Instance().All(); }

#define FetchAndValidateItemTable(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
do {if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomItemTable(prefix, keyId) \
const auto [##prefix##ItemTable, prefix##fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##ItemTable)) { LOG_ERROR << "Item table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchItemTableOrReturnCustom(keyId, customReturnValue) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
do {if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchItemTableOrReturnVoid(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
do {if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << keyId;return ;}} while (0)

#define FetchItemTableOrContinue(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << keyId;continue; }} while (0)

#define FetchItemTableOrReturnFalse(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
do {if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << keyId;return false; }} while (0)