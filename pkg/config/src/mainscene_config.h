#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "mainscene_config.pb.h"


class MainSceneConfigurationTable {
public:
    using KVDataType = std::unordered_map<uint32_t, const MainSceneTable*>;
    static MainSceneConfigurationTable& GetSingleton() { static MainSceneConfigurationTable singleton; return singleton; }
    const MainSceneTabledData& All() const { return data_; }
    std::pair<const MainSceneTable*, uint32_t> GetTable(uint32_t keyid);
    const KVDataType& KVData() const { return kv_data_; }
    void Load();

private:
    MainSceneTabledData data_;
    KVDataType kv_data_;

};

inline std::pair<const MainSceneTable*, uint32_t> GetMainSceneTable(uint32_t keyid) { return MainSceneConfigurationTable::GetSingleton().GetTable(keyid); }

inline const MainSceneTabledData& GetMainSceneAllTable() { return MainSceneConfigurationTable::GetSingleton().All(); }