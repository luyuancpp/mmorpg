#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "buff_config.pb.h"


class BuffConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const BuffTable*>;
    static BuffConfigurationTable& GetSingleton() { static BuffConfigurationTable singleton; return singleton; }
    const BuffTabledData& All() const { return data_; }
    std::pair<const BuffTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    BuffTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const BuffTable*, uint32_t> GetBuffTable(uint32_t keyid) { return BuffConfigurationTable::GetSingleton().GetTable(keyid); }

inline const BuffTabledData& GetBuffAllTable() { return BuffConfigurationTable::GetSingleton().All(); }