#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "scene_config.pb.h"


class SceneConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const SceneTable*>;
    static SceneConfigurationTable& GetSingleton() { static SceneConfigurationTable singleton; return singleton; }
    const SceneTabledData& All() const { return data_; }
    std::pair<const SceneTable*, uint32_t> GetTable(uint32_t keyId);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();


private:
    SceneTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const SceneTable*, uint32_t> GetSceneTable(const uint32_t keyId) { return SceneConfigurationTable::GetSingleton().GetTable(keyId); }

inline const SceneTabledData& GetSceneAllTable() { return SceneConfigurationTable::GetSingleton().All(); }