#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "skill_config.pb.h"


class SkillConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const SkillTable*>;
    static SkillConfigurationTable& GetSingleton() { static SkillConfigurationTable singleton; return singleton; }
    const SkillTabledData& All() const { return data_; }
    std::pair<const SkillTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    SkillTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const SkillTable*, uint32_t> GetSkillTable(uint32_t keyid) { return SkillConfigurationTable::GetSingleton().GetTable(keyid); }

inline const SkillTabledData& GetSkillAllTable() { return SkillConfigurationTable::GetSingleton().All(); }