#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "monsterbase_config.pb.h"


class MonsterBaseConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const MonsterBaseTable*>;
    static MonsterBaseConfigurationTable& GetSingleton() { static MonsterBaseConfigurationTable singleton; return singleton; }
    const MonsterBaseTabledData& All() const { return data_; }
    std::pair<const MonsterBaseTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    MonsterBaseTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const MonsterBaseTable*, uint32_t> GetMonsterBaseTable(uint32_t keyid) { return MonsterBaseConfigurationTable::GetSingleton().GetTable(keyid); }

inline const MonsterBaseTabledData& GetMonsterBaseAllTable() { return MonsterBaseConfigurationTable::GetSingleton().All(); }