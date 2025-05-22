#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "actoractioncombatstate_config.pb.h"

class ActorActionCombatStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionCombatStateTable*>;

    static ActorActionCombatStateConfigurationTable& Instance() {
        static ActorActionCombatStateConfigurationTable instance;
        return instance;
    }

    const ActorActionCombatStateTabledData& All() const { return data_; }
    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    ActorActionCombatStateTabledData data_;
    KeyValueDataType kv_data_;
};

inline const ActorActionCombatStateTabledData& GetActorActionCombatStateAllTable() {
    return ActorActionCombatStateConfigurationTable::Instance().All();
}

#define FetchAndValidateActorActionCombatStateTable(tableId) \
    const auto [actoractioncombatstateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractioncombatstateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionCombatStateTable(prefix, tableId) \
    const auto [prefix##ActorActionCombatStateTable, prefix##fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionCombatStateTable)) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionCombatStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actoractioncombatstateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractioncombatstateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnVoid(tableId) \
    const auto [actoractioncombatstateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractioncombatstateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionCombatStateTableOrContinue(tableId) \
    const auto [actoractioncombatstateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractioncombatstateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionCombatStateTableOrReturnFalse(tableId) \
    const auto [actoractioncombatstateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractioncombatstateTable )) { LOG_ERROR << "ActorActionCombatState table not found for ID: " << tableId; return false; } } while(0)