#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "actoractioncombatstate_config.pb.h"

class ActorActionCombatStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionCombatStateTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ActorActionCombatStateConfigurationTable& Instance() {
        static ActorActionCombatStateConfigurationTable instance;
        return instance;
    }

    const ActorActionCombatStateTabledData& All() const { return data_; }

    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ActorActionCombatStateTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ActorActionCombatStateTabledData& GetActorActionCombatStateAllTable() {
    return ActorActionCombatStateConfigurationTable::Instance().All();
}

#define FetchAndValidateActorActionCombatStateTable(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionCombatStateTable(prefix, tableId) \
    const auto [prefix##ActorActionCombatStateTable, prefix##fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionCombatStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnVoid(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionCombatStateTableOrContinue(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnFalse(tableId) \
    const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionCombatStateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return false; } } while(0)