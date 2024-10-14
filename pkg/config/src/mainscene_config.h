#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "mainscene_config.pb.h"


class MainSceneConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MainSceneTable*>;
    static MainSceneConfigurationTable& Instance() { static MainSceneConfigurationTable instance; return instance; }
    const MainSceneTabledData& All() const { return data_; }
    std::pair<const MainSceneTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    MainSceneTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const MainSceneTable*, uint32_t> GetMainSceneTable(const uint32_t keyId) { return MainSceneConfigurationTable::Instance().GetTable(keyId); }

inline const MainSceneTabledData& GetMainSceneAllTable() { return MainSceneConfigurationTable::Instance().All(); }