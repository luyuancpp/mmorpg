#pragma once
#include <memory>
#include <unordered_map>
#include "ability_config.pb.h"
class AbilityConfigurationTable
{
public:
    using row_type = const ability_row*;
    using kv_type = std::unordered_map<uint32_t, row_type>;
    static AbilityConfigurationTable& GetSingleton() { static AbilityConfigurationTable singleton; return singleton; }
    const ability_table& All() const { return data_; }
    const std::pair<row_type, uint32_t> GetTable(uint32_t keyid);
    void Load();
private:
    ability_table data_;
    kv_type key_data_;
};

inline std::pair< AbilityConfigurationTable::row_type, uint32_t> GetAbilityTable(uint32_t keyid)
{
    return AbilityConfigurationTable::GetSingleton().GetTable(keyid);
}

inline  const ability_table& GetAbilityAllTable()
{
    return AbilityConfigurationTable::GetSingleton().All();
}
