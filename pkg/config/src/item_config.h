#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
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

inline std::pair<const ItemTable*, uint32_t> GetItemTable(const uint32_t keyId) { return ItemConfigurationTable::Instance().GetTable(keyId); }

inline const ItemTabledData& GetItemAllTable() { return ItemConfigurationTable::Instance().All(); }

#define FetchAndValidateItemTable(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
if (!(itemTable)) { return (fetchResult); }

#define FetchItemTableOrReturnVoid(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
if (!(itemTable)) { return ;}

#define FetchItemTableOrContinue(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
if (!(itemTable)) { continue; }

#define FetchItemTableOrReturnFalse(keyId) \
const auto [itemTable, fetchResult] = ItemConfigurationTable::Instance().GetTable(keyId); \
if (!(itemTable)) { return false; }