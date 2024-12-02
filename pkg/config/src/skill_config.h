#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "skill_config.pb.h"


class SkillConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillTable*>;
    static SkillConfigurationTable& Instance() { static SkillConfigurationTable instance; return instance; }
    const SkillTabledData& All() const { return data_; }
    std::pair<const SkillTable*, uint32_t> GetTable(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();

    void SetDamageParam(const std::vector<double>& paramList){
      expression_damage_.SetParam(paramList); 
    }
 
    double GetDamage(const uint32_t tableId){
      auto [table, ok] = GetTable(tableId);
      if ( table == nullptr){return double(); }
      return expression_damage_.Value(table->damage());
     } 

private:
    SkillTabledData data_;
    KeyValueDataType kv_data_;

    ExcelExpression<double> expression_damage_;
};

inline const SkillTabledData& GetSkillAllTable() { return SkillConfigurationTable::Instance().All(); }

#define FetchAndValidateSkillTable(tableId) \
const auto [skillTable, fetchResult] = SkillConfigurationTable::Instance().GetTable(tableId); \
do {if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomSkillTable(prefix, tableId) \
const auto [##prefix##SkillTable, prefix##fetchResult] = SkillConfigurationTable::Instance().GetTable(tableId); \
do {if (!(##prefix##SkillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId;return (prefix##fetchResult); }} while (0)

#define FetchSkillTableOrReturnCustom(tableId, customReturnValue) \
const auto [skillTable, fetchResult] = SkillConfigurationTable::Instance().GetTable(tableId); \
do {if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId;return (customReturnValue); }} while (0)

#define FetchSkillTableOrReturnVoid(tableId) \
const auto [skillTable, fetchResult] = SkillConfigurationTable::Instance().GetTable(tableId); \
do {if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId;return ;}} while (0)

#define FetchSkillTableOrContinue(tableId) \
const auto [skillTable, fetchResult] = SkillConfigurationTable::Instance().GetTable(tableId); \
do { if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId;continue; }} while (0)

#define FetchSkillTableOrReturnFalse(tableId) \
const auto [skillTable, fetchResult] = SkillConfigurationTable::Instance().GetTable(tableId); \
do {if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId;return false; }} while (0)