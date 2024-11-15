#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "cooldown_config.pb.h"


class CooldownConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const CooldownTable*>;
    static CooldownConfigurationTable& Instance() { static CooldownConfigurationTable instance; return instance; }
    const CooldownTabledData& All() const { return data_; }
    std::pair<const CooldownTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    CooldownTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const CooldownTable*, uint32_t> GetCooldownTable(const uint32_t keyId) { return CooldownConfigurationTable::Instance().GetTable(keyId); }

inline const CooldownTabledData& GetCooldownAllTable() { return CooldownConfigurationTable::Instance().All(); }

#define FetchAndValidateCooldownTable(keyId) \
const auto [cooldownTable, result] = CooldownConfigurationTable::Instance().GetTable(keyId); \
if (!(cooldownTable)) { return (result); }

#define FetchCooldownTableOrReturnVoid(keyId) \
const auto [cooldownTable, result] = CooldownConfigurationTable::Instance().GetTable(keyId); \
if (!(cooldownTable)) { return ;}