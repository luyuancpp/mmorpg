#pragma once
#include <cstdint>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "proto/table/actoractionstate_table.pb.h"

class ActorActionStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionStateTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static ActorActionStateConfigurationTable& Instance() {
        static ActorActionStateConfigurationTable instance;
        return instance;
    }

    const ActorActionStateTabledData& All() const { return data_; }

    std::pair<const ActorActionStateTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ActorActionStateTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    ActorActionStateTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const ActorActionStateTabledData& GetActorActionStateAllTable() {
    return ActorActionStateConfigurationTable::Instance().All();
}

#define FetchAndValidateActorActionStateTable(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionStateTable(prefix, tableId) \
    const auto [prefix##ActorActionStateTable, prefix##fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##ActorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionStateTableOrReturnVoid(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionStateTableOrContinue(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionStateTableOrReturnFalse(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( actorActionStateTable )) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return false; } } while(0)