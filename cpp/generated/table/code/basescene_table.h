#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/basescene_table.pb.h"

class BaseSceneTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const BaseSceneTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static BaseSceneTableManager& Instance() {
        static BaseSceneTableManager instance;
        return instance;
    }

    const BaseSceneTableData& All() const { return data_; }

    std::pair<const BaseSceneTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const BaseSceneTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

private:
    LoadSuccessCallback loadSuccessCallback_;
    BaseSceneTableData data_;
    KeyValueDataType kv_data_;
};

inline const BaseSceneTableData& GetBaseSceneAllTable() {
    return BaseSceneTableManager::Instance().All();
}

#define FetchAndValidateBaseSceneTable(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBaseSceneTable(prefix, tableId) \
    const auto [prefix##BaseSceneTable, prefix##fetchResult] = BaseSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##BaseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBaseSceneTableOrReturnCustom(tableId, customReturnValue) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBaseSceneTableOrReturnVoid(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return; } } while(0)

#define FetchBaseSceneTableOrContinue(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; continue; } } while(0)

#define FetchBaseSceneTableOrReturnFalse(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().GetTable(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return false; } } while(0)
