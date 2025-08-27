#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression.h"
#include "muduo/base/Logging.h"
#include "scene_config.pb.h"

class SceneConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SceneTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static SceneConfigurationTable& Instance() {
        static SceneConfigurationTable instance;
        return instance;
    }

    const SceneTabledData& All() const { return data_; }

    std::pair<const SceneTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const SceneTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    SceneTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const SceneTabledData& GetSceneAllTable() {
    return SceneConfigurationTable::Instance().All();
}

#define FetchAndValidateSceneTable(tableId) \
    const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSceneTable(prefix, tableId) \
    const auto [prefix##SceneTable, prefix##fetchResult] = SceneConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##SceneTable)) { LOG_ERROR << "Scene table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSceneTableOrReturnCustom(tableId, customReturnValue) \
    const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSceneTableOrReturnVoid(tableId) \
    const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return; } } while(0)

#define FetchSceneTableOrContinue(tableId) \
    const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; continue; } } while(0)

#define FetchSceneTableOrReturnFalse(tableId) \
    const auto [sceneTable, fetchResult] = SceneConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( sceneTable )) { LOG_ERROR << "Scene table not found for ID: " << tableId; return false; } } while(0)