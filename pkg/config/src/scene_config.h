#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
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

inline const SceneTabledData& GetSceneAllTable() { return SceneConfigurationTable::Instance().All(); }

#define FetchAndValidateSceneTable(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
do {if (!(sceneTable)) { LOG_ERROR << "Scene table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomSceneTable(prefix, keyId) \
const auto [##prefix##SceneTable, prefix##fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##SceneTable)) { LOG_ERROR << "Scene table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchSceneTableOrReturnCustom(keyId, customReturnValue) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
do {if (!(sceneTable)) { LOG_ERROR << "Scene table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchSceneTableOrReturnVoid(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
do {if (!(sceneTable)) { LOG_ERROR << "Scene table not found for ID: " << keyId;return ;}} while (0)

#define FetchSceneTableOrContinue(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
do { if (!(sceneTable)) { LOG_ERROR << "Scene table not found for ID: " << keyId;continue; }} while (0)

#define FetchSceneTableOrReturnFalse(keyId) \
const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(keyId); \
do {if (!(sceneTable)) { LOG_ERROR << "Scene table not found for ID: " << keyId;return false; }} while (0)