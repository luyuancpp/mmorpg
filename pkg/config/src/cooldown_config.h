#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "cooldown_config.pb.h"


class CooldownConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const CooldownTable*>;
    static CooldownConfigurationTable& GetSingleton() { static CooldownConfigurationTable singleton; return singleton; }
    const CooldownTabledData& All() const { return data_; }
    std::pair<const CooldownTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    CooldownTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const CooldownTable*, uint32_t> GetCooldownTable(uint32_t keyid) { return CooldownConfigurationTable::GetSingleton().GetTable(keyid); }

inline const CooldownTabledData& GetCooldownAllTable() { return CooldownConfigurationTable::GetSingleton().All(); }