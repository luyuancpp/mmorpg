#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression.h"
#include "muduo/base/Logging.h"
#include "proto/config/cooldown_config.pb.h"

class CooldownConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const CooldownTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static CooldownConfigurationTable& Instance() {
        static CooldownConfigurationTable instance;
        return instance;
    }

    const CooldownTabledData& All() const { return data_; }

    std::pair<const CooldownTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const CooldownTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    CooldownTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const CooldownTabledData& GetCooldownAllTable() {
    return CooldownConfigurationTable::Instance().All();
}

#define FetchAndValidateCooldownTable(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomCooldownTable(prefix, tableId) \
    const auto [prefix##CooldownTable, prefix##fetchResult] = CooldownConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##CooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchCooldownTableOrReturnCustom(tableId, customReturnValue) \
    const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchCooldownTableOrReturnVoid(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return; } } while(0)

#define FetchCooldownTableOrContinue(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; continue; } } while(0)

#define FetchCooldownTableOrReturnFalse(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( cooldownTable )) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return false; } } while(0)