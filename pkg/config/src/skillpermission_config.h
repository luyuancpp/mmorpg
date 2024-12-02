#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
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

inline const SkillPermissionTabledData& GetSkillPermissionAllTable() { return SkillPermissionConfigurationTable::Instance().All(); }

#define FetchAndValidateSkillPermissionTable(keyId) \
const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomSkillPermissionTable(prefix, keyId) \
const auto [##prefix##SkillPermissionTable, prefix##fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##SkillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchSkillPermissionTableOrReturnCustom(keyId, customReturnValue) \
const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchSkillPermissionTableOrReturnVoid(keyId) \
const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << keyId;return ;}} while (0)

#define FetchSkillPermissionTableOrContinue(keyId) \
const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(keyId); \
do { if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << keyId;continue; }} while (0)

#define FetchSkillPermissionTableOrReturnFalse(keyId) \
const auto [skillPermissionTable, fetchResult] = SkillPermissionConfigurationTable::Instance().GetTable(keyId); \
do {if (!(skillPermissionTable)) { LOG_ERROR << "SkillPermission table not found for ID: " << keyId;return false; }} while (0)