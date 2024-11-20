#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "monsterbase_config.pb.h"


class MonsterBaseConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MonsterBaseTable*>;
    static MonsterBaseConfigurationTable& Instance() { static MonsterBaseConfigurationTable instance; return instance; }
    const MonsterBaseTabledData& All() const { return data_; }
    std::pair<const MonsterBaseTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    MonsterBaseTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const MonsterBaseTable*, uint32_t> GetMonsterBaseTable(const uint32_t keyId) { return MonsterBaseConfigurationTable::Instance().GetTable(keyId); }

inline const MonsterBaseTabledData& GetMonsterBaseAllTable() { return MonsterBaseConfigurationTable::Instance().All(); }

#define FetchAndValidateMonsterBaseTable(keyId) \
const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(keyId); \
if (!(monsterBaseTable)) { return (fetchResult); }

#define FetchMonsterBaseTableOrReturnVoid(keyId) \
const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(keyId); \
do {if (!(monsterBaseTable)) { return ;}} while (0)

#define FetchMonsterBaseTableOrContinue(keyId) \
const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(keyId); \
do { if (!(monsterBaseTable)) { continue; }} while (0)

#define FetchMonsterBaseTableOrReturnFalse(keyId) \
const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(keyId); \
do {if (!(monsterBaseTable)) { return false; }} while (0)