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


private:
    BuffTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const BuffTable*, uint32_t> GetBuffTable(const uint32_t keyId) { return BuffConfigurationTable::Instance().GetTable(keyId); }

inline const BuffTabledData& GetBuffAllTable() { return BuffConfigurationTable::Instance().All(); }