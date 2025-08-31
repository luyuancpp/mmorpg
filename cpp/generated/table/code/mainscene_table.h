#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/mainscene_table.pb.h"

class MainSceneTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MainSceneTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static MainSceneTableManager& Instance() {
        static MainSceneTableManager instance;
        return instance;
    }

    const MainSceneTabledData& All() const { return data_; }

    std::pair<const MainSceneTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MainSceneTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    MainSceneTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const MainSceneTabledData& GetMainSceneAllTable() {
    return MainSceneTableManager::Instance().All();
}

#define FetchAndValidateMainSceneTable(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMainSceneTable(prefix, tableId) \
    const auto [prefix##MainSceneTable, prefix##fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MainSceneTable)) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMainSceneTableOrReturnCustom(tableId, customReturnValue) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMainSceneTableOrReturnVoid(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return; } } while(0)

#define FetchMainSceneTableOrContinue(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; continue; } } while(0)

#define FetchMainSceneTableOrReturnFalse(tableId) \
    const auto [mainSceneTable, fetchResult] = MainSceneTableManager::Instance().GetTable(tableId); \
    do { if (!( mainSceneTable )) { LOG_ERROR << "MainScene table not found for ID: " << tableId; return false; } } while(0)