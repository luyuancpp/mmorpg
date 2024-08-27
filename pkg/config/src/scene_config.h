#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "scene_config.pb.h"


class SceneConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const SceneTable*>;
    static SceneConfigurationTable& GetSingleton() { static SceneConfigurationTable singleton; return singleton; }
    const SceneTabledData& All() const { return data_; }
    std::pair<const SceneTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    SceneTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const SceneTable*, uint32_t> GetSceneTable(uint32_t keyid) { return SceneConfigurationTable::GetSingleton().GetTable(keyid); }

inline const SceneTabledData& GetSceneAllTable() { return SceneConfigurationTable::GetSingleton().All(); }