#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "scene_config.pb.h"


class SceneConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SceneTable*>;
    static SceneConfigurationTable& Instance() { static SceneConfigurationTable instance; return instance; }
    const SceneTabledData& All() const { return data_; }
    std::pair<const SceneTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    SceneTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const SceneTable*, uint32_t> GetSceneTable(const uint32_t keyId) { return SceneConfigurationTable::Instance().GetTable(keyId); }

inline const SceneTabledData& GetSceneAllTable() { return SceneConfigurationTable::Instance().All(); }

#define FetchAndValidateSceneTable(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
if (!(sceneTable)) { return (fetchResult); }

#define FetchSceneTableOrReturnVoid(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
if (!(sceneTable)) { return ;}

#define FetchSceneTableOrContinue(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
if (!(sceneTable)) { continue; }

#define FetchSceneTableOrReturnFalse(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
if (!(sceneTable)) { return false; }