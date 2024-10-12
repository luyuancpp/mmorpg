#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "cooldown_config.pb.h"


class CooldownConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const CooldownTable*>;
    static CooldownConfigurationTable& GetSingleton() { static CooldownConfigurationTable singleton; return singleton; }
    const CooldownTabledData& All() const { return data_; }
    std::pair<const CooldownTable*, uint32_t> GetTable(uint32_t keyId);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();


private:
    CooldownTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const CooldownTable*, uint32_t> GetCooldownTable(const uint32_t keyId) { return CooldownConfigurationTable::GetSingleton().GetTable(keyId); }

inline const CooldownTabledData& GetCooldownAllTable() { return CooldownConfigurationTable::GetSingleton().All(); }