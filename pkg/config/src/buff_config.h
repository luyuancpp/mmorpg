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

private:
    BuffTabledData data_;
    KeyValueDataType kv_data_;

    ExcelExpression<double> expression_healthregeneration_;
};

inline std::pair<const BuffTable*, uint32_t> GetBuffTable(const uint32_t keyId) { return BuffConfigurationTable::Instance().GetTable(keyId); }

inline const BuffTabledData& GetBuffAllTable() { return BuffConfigurationTable::Instance().All(); }