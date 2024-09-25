#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "item_config.pb.h"


class ItemConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const ItemTable*>;
    static ItemConfigurationTable& GetSingleton() { static ItemConfigurationTable singleton; return singleton; }
    const ItemTabledData& All() const { return data_; }
    std::pair<const ItemTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    ItemTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const ItemTable*, uint32_t> GetItemTable(uint32_t keyid) { return ItemConfigurationTable::GetSingleton().GetTable(keyid); }

inline const ItemTabledData& GetItemAllTable() { return ItemConfigurationTable::GetSingleton().All(); }