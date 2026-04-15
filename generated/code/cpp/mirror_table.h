#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/mirror_table.pb.h"

class MirrorTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MirrorTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static MirrorTableManager& Instance() {
        static MirrorTableManager instance;
        return instance;
    }

    const MirrorTableData& All() const { return data_; }

    std::pair<const MirrorTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MirrorTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // FK: scene_id → BaseScene.id
    // FK: main_scene_id → World.id

private:
    LoadSuccessCallback loadSuccessCallback_;
    MirrorTableData data_;
    KeyValueDataType kv_data_;
};

inline const MirrorTableData& GetMirrorAllTable() {
    return MirrorTableManager::Instance().All();
}

#define FetchAndValidateMirrorTable(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMirrorTable(prefix, tableId) \
    const auto [prefix##MirrorTable, prefix##fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMirrorTableOrReturnCustom(tableId, customReturnValue) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMirrorTableOrReturnVoid(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return; } } while(0)

#define FetchMirrorTableOrContinue(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; continue; } } while(0)

#define FetchMirrorTableOrReturnFalse(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return false; } } while(0)
