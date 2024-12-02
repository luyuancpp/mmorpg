#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "mainscene_config.pb.h"


class MainSceneConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MainSceneTable*>;
    static MainSceneConfigurationTable& Instance() { static MainSceneConfigurationTable instance; return instance; }
    const MainSceneTabledData& All() const { return data_; }
    std::pair<const MainSceneTable*, uint32_t> GetTable(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    MainSceneTabledData data_;
    KeyValueDataType kv_data_;

};

inline const MainSceneTabledData& GetMainSceneAllTable() { return MainSceneConfigurationTable::Instance().All(); }

#define FetchAndValidateMainSceneTable(tableId) \
const auto [mainSceneTable, fetchResult] = MainSceneConfigurationTable::Instance().GetTable(tableId); \
do {if (!(mainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomMainSceneTable(prefix, tableId) \
const auto [##prefix##MainSceneTable, prefix##fetchResult] = MainSceneConfigurationTable::Instance().GetTable(tableId); \
do {if (!(##prefix##MainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId;return (prefix##fetchResult); }} while (0)

#define FetchMainSceneTableOrReturnCustom(tableId, customReturnValue) \
const auto [mainSceneTable, fetchResult] = MainSceneConfigurationTable::Instance().GetTable(tableId); \
do {if (!(mainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId;return (customReturnValue); }} while (0)

#define FetchMainSceneTableOrReturnVoid(tableId) \
const auto [mainSceneTable, fetchResult] = MainSceneConfigurationTable::Instance().GetTable(tableId); \
do {if (!(mainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId;return ;}} while (0)

#define FetchMainSceneTableOrContinue(tableId) \
const auto [mainSceneTable, fetchResult] = MainSceneConfigurationTable::Instance().GetTable(tableId); \
do { if (!(mainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId;continue; }} while (0)

#define FetchMainSceneTableOrReturnFalse(tableId) \
const auto [mainSceneTable, fetchResult] = MainSceneConfigurationTable::Instance().GetTable(tableId); \
do {if (!(mainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId;return false; }} while (0)