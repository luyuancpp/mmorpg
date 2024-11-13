#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "skillpermission_config.pb.h"


class SkillPermissionConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillPermissionTable*>;
    static SkillPermissionConfigurationTable& Instance() { static SkillPermissionConfigurationTable instance; return instance; }
    const SkillPermissionTabledData& All() const { return data_; }
    std::pair<const SkillPermissionTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    SkillPermissionTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const SkillPermissionTable*, uint32_t> GetSkillPermissionTable(const uint32_t keyId) { return SkillPermissionConfigurationTable::Instance().GetTable(keyId); }

inline const SkillPermissionTabledData& GetSkillPermissionAllTable() { return SkillPermissionConfigurationTable::Instance().All(); }