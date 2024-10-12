#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "class_config.pb.h"


class ClassConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const ClassTable*>;
    static ClassConfigurationTable& GetSingleton() { static ClassConfigurationTable singleton; return singleton; }
    const ClassTabledData& All() const { return data_; }
    std::pair<const ClassTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    ClassTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const ClassTable*, uint32_t> GetClassTable(uint32_t keyid) { return ClassConfigurationTable::GetSingleton().GetTable(keyid); }

inline const ClassTabledData& GetClassAllTable() { return ClassConfigurationTable::GetSingleton().All(); }