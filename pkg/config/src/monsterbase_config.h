#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "monsterbase_config.pb.h"


class MonsterBaseConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MonsterBaseTable*>;
    static MonsterBaseConfigurationTable& GetSingleton() { static MonsterBaseConfigurationTable singleton; return singleton; }
    const MonsterBaseTabledData& All() const { return data_; }
    std::pair<const MonsterBaseTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    MonsterBaseTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const MonsterBaseTable*, uint32_t> GetMonsterBaseTable(const uint32_t keyId) { return MonsterBaseConfigurationTable::GetSingleton().GetTable(keyId); }

inline const MonsterBaseTabledData& GetMonsterBaseAllTable() { return MonsterBaseConfigurationTable::GetSingleton().All(); }