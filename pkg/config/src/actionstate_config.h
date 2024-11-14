#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "actionstate_config.pb.h"


class ActionStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActionStateTable*>;
    static ActionStateConfigurationTable& Instance() { static ActionStateConfigurationTable instance; return instance; }
    const ActionStateTabledData& All() const { return data_; }
    std::pair<const ActionStateTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ActionStateTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const ActionStateTable*, uint32_t> GetActionStateTable(const uint32_t keyId) { return ActionStateConfigurationTable::Instance().GetTable(keyId); }

inline const ActionStateTabledData& GetActionStateAllTable() { return ActionStateConfigurationTable::Instance().All(); }

#define FetchAndValidateActionStateTable(keyId) \
const auto [actionStateTable, result] = ActionStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actionStateTable)) { return (result); }

#define FetchActionStateTableOrReturnVoid(keyId) \
const auto [actionStateTable, result] = ActionStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actionStateTable)) { return  }