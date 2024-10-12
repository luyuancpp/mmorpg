#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "item_config.pb.h"


class ItemConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;
    static ItemConfigurationTable& GetSingleton() { static ItemConfigurationTable singleton; return singleton; }
    const ItemTabledData& All() const { return data_; }
    std::pair<const ItemTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ItemTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const ItemTable*, uint32_t> GetItemTable(const uint32_t keyId) { return ItemConfigurationTable::GetSingleton().GetTable(keyId); }

inline const ItemTabledData& GetItemAllTable() { return ItemConfigurationTable::GetSingleton().All(); }