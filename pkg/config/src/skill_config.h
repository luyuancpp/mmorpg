#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "skill_config.pb.h"


class SkillConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const SkillTable*>;
    static SkillConfigurationTable& GetSingleton() { static SkillConfigurationTable singleton; return singleton; }
    const SkillTabledData& All() const { return data_; }
    std::pair<const SkillTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();
    double GetDamage(const uint32_t keyid){
          auto [table, ok] = GetTable(keyid);
          if ( table == nullptr){{return double(); }}
          return expression_damage_.Value(table->damage());
     } 

private:
    SkillTabledData data_;
    KVDataType kv_data_;

    ExcelExpression<double> expression_damage_;
};

inline std::pair<const SkillTable*, uint32_t> GetSkillTable(const uint32_t keyid) { return SkillConfigurationTable::GetSingleton().GetTable(keyid); }

inline const SkillTabledData& GetSkillAllTable() { return SkillConfigurationTable::GetSingleton().All(); }