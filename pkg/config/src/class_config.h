#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "class_config.pb.h"


class ClassConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ClassTable*>;
    static ClassConfigurationTable& Instance() { static ClassConfigurationTable instance; return instance; }
    const ClassTabledData& All() const { return data_; }
    std::pair<const ClassTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ClassTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const ClassTable*, uint32_t> GetClassTable(const uint32_t keyId) { return ClassConfigurationTable::Instance().GetTable(keyId); }

inline const ClassTabledData& GetClassAllTable() { return ClassConfigurationTable::Instance().All(); }