#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "actoractionstate_config.pb.h"

class ActorActionStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionStateTable*>;

    static ActorActionStateConfigurationTable& Instance() {
        static ActorActionStateConfigurationTable instance;
        return instance;
    }

    const ActorActionStateTabledData& All() const { return data_; }
    std::pair<const ActorActionStateTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ActorActionStateTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    ActorActionStateTabledData data_;
    KeyValueDataType kv_data_;
};

inline const ActorActionStateTabledData& GetActorActionStateAllTable() {
    return ActorActionStateConfigurationTable::Instance().All();
}

#define FetchAndValidateActorActionStateTable(tableId) \
    const auto [actoractionstateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractionstateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionStateTable(prefix, tableId) \
    const auto [prefix##ActorActionStateTable, prefix##fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actoractionstateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractionstateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionStateTableOrReturnVoid(tableId) \
    const auto [actoractionstateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractionstateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionStateTableOrContinue(tableId) \
    const auto [actoractionstateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractionstateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionStateTableOrReturnFalse(tableId) \
    const auto [actoractionstateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actoractionstateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return false; } } while(0)