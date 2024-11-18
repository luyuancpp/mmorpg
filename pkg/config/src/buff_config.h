#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "buff_config.pb.h"


class BuffConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const BuffTable*>;
    static BuffConfigurationTable& Instance() { static BuffConfigurationTable instance; return instance; }
    const BuffTabledData& All() const { return data_; }
    std::pair<const BuffTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();

    void SetHealthregenerationParam(const std::vector<double>& paramList){
      expression_healthregeneration_.SetParam(paramList); 
    }
 
    double GetHealthregeneration(const uint32_t keyId){
      auto [table, ok] = GetTable(keyId);
      if ( table == nullptr){return double(); }
      return expression_healthregeneration_.Value(table->healthregeneration());
     } 
    void SetBonusdamageParam(const std::vector<double>& paramList){
      expression_bonusdamage_.SetParam(paramList); 
    }
 
    double GetBonusdamage(const uint32_t keyId){
      auto [table, ok] = GetTable(keyId);
      if ( table == nullptr){return double(); }
      return expression_bonusdamage_.Value(table->bonusdamage());
     } 

private:
    BuffTabledData data_;
    KeyValueDataType kv_data_;

    ExcelExpression<double> expression_healthregeneration_;
    ExcelExpression<double> expression_bonusdamage_;
};

inline std::pair<const BuffTable*, uint32_t> GetBuffTable(const uint32_t keyId) { return BuffConfigurationTable::Instance().GetTable(keyId); }

inline const BuffTabledData& GetBuffAllTable() { return BuffConfigurationTable::Instance().All(); }

#define FetchAndValidateBuffTable(keyId) \
const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(keyId); \
if (!(buffTable)) { return (fetchResult); }

#define FetchBuffTableOrReturnVoid(keyId) \
const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(keyId); \
if (!(buffTable)) { return ;}

#define FetchBuffTableOrContinue(keyId) \
const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(keyId); \
if (!(buffTable)) { continue; }

#define FetchBuffTableOrReturnFalse(keyId) \
const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(keyId); \
if (!(buffTable)) { return false; }