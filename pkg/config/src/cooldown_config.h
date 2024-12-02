#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
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

inline const CooldownTabledData& GetCooldownAllTable() { return CooldownConfigurationTable::Instance().All(); }

#define FetchAndValidateCooldownTable(keyId) \
const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(keyId); \
do {if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomCooldownTable(prefix, keyId) \
const auto [##prefix##CooldownTable, prefix##fetchResult] = CooldownConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##CooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchCooldownTableOrReturnCustom(keyId, customReturnValue) \
const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(keyId); \
do {if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchCooldownTableOrReturnVoid(keyId) \
const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(keyId); \
do {if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << keyId;return ;}} while (0)

#define FetchCooldownTableOrContinue(keyId) \
const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(keyId); \
do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << keyId;continue; }} while (0)

#define FetchCooldownTableOrReturnFalse(keyId) \
const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(keyId); \
do {if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << keyId;return false; }} while (0)