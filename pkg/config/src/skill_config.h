#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "skill_config.pb.h"


class SkillConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillTable*>;
    static SkillConfigurationTable& Instance() { static SkillConfigurationTable instance; return instance; }
    const SkillTabledData& All() const { return data_; }
    std::pair<const SkillTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();

    void SetDamageParam(const std::vector<double>& paramList){
      expression_damage_.SetParam(paramList); 
    }
 
    double GetDamage(const uint32_t keyId){
      auto [table, ok] = GetTable(keyId);
      if ( table == nullptr){return double(); }
      return expression_damage_.Value(table->damage());
     } 

private:
    SkillTabledData data_;
    KeyValueDataType kv_data_;

    ExcelExpression<double> expression_damage_;
};

inline std::pair<const SkillTable*, uint32_t> GetSkillTable(const uint32_t keyId) { return SkillConfigurationTable::Instance().GetTable(keyId); }

inline const SkillTabledData& GetSkillAllTable() { return SkillConfigurationTable::Instance().All(); }

#define FetchAndValidateSkillTable(keyId) \
const auto [skillTable, result] = SkillConfigurationTable::Instance().GetTable(keyId); \
if (!(skillTable)) { return (result); }

#define FetchSkillTableOrReturnVoid(keyId) \
const auto [skillTable, result] = SkillConfigurationTable::Instance().GetTable(keyId); \
if (!(skillTable)) { return ;}