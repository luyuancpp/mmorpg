#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "globalvariable_config.pb.h"


class GlobalVariableConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;
    static GlobalVariableConfigurationTable& GetSingleton() { static GlobalVariableConfigurationTable singleton; return singleton; }
    const GlobalVariableTabledData& All() const { return data_; }
    std::pair<const GlobalVariableTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    GlobalVariableTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const GlobalVariableTable*, uint32_t> GetGlobalVariableTable(uint32_t keyid) { return GlobalVariableConfigurationTable::GetSingleton().GetTable(keyid); }

inline const GlobalVariableTabledData& GetGlobalVariableAllTable() { return GlobalVariableConfigurationTable::GetSingleton().All(); }