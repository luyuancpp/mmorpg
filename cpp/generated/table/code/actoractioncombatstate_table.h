
#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/actoractioncombatstate_table.pb.h"

class ActorActionCombatStateTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionCombatStateTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static ActorActionCombatStateTableManager& Instance() {
        static ActorActionCombatStateTableManager instance;
        return instance;
    }

    const ActorActionCombatStateTableData& All() const { return data_; }

    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }








private:
    LoadSuccessCallback loadSuccessCallback_;
    ActorActionCombatStateTableData data_;
    KeyValueDataType kv_data_;



};

inline const ActorActionCombatStateTableData& GetActorActionCombatStateAllTable() {
    return ActorActionCombatStateTableManager::Instance().All();
}

#define FetchAndValidateActorActionCombatStateTable(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(actorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionCombatStateTable(prefix, tableId) \
    const auto [prefix##ActorActionCombatStateTable, prefix##fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionCombatStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(actorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnVoid(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(actorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionCombatStateTableOrContinue(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(actorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnFalse(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateTableManager::Instance().GetTable(tableId); \
    do { if (!(actorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return false; } } while(0)