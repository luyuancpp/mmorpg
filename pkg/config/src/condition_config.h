#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "condition_config.pb.h"


class ConditionConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const ConditionTable*>;
    static ConditionConfigurationTable& GetSingleton() { static ConditionConfigurationTable singleton; return singleton; }
    const ConditionTabledData& All() const { return data_; }
    std::pair<const ConditionTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    ConditionTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const ConditionTable*, uint32_t> GetConditionTable(uint32_t keyid) { return ConditionConfigurationTable::GetSingleton().GetTable(keyid); }

inline const ConditionTabledData& GetConditionAllTable() { return ConditionConfigurationTable::GetSingleton().All(); }