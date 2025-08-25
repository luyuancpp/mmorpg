#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression.h"
#include "muduo/base/Logging.h"
#include "proto/config/monsterbase_config.pb.h"

class MonsterBaseConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MonsterBaseTable*>;

    // Callback type definition
    using LoadSuccessCallback = std::function<void()>;

    static MonsterBaseConfigurationTable& Instance() {
        static MonsterBaseConfigurationTable instance;
        return instance;
    }

    const MonsterBaseTabledData& All() const { return data_; }

    std::pair<const MonsterBaseTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MonsterBaseTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    // Setter for the success callback
    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;//multi thread
    }

    void LoadSuccess(){if (loadSuccessCallback_){loadSuccessCallback_();}}

    

private:
    LoadSuccessCallback loadSuccessCallback_;  // The callback for load success
    MonsterBaseTabledData data_;
    KeyValueDataType kv_data_;
    
};

inline const MonsterBaseTabledData& GetMonsterBaseAllTable() {
    return MonsterBaseConfigurationTable::Instance().All();
}

#define FetchAndValidateMonsterBaseTable(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMonsterBaseTable(prefix, tableId) \
    const auto [prefix##MonsterBaseTable, prefix##fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##MonsterBaseTable)) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMonsterBaseTableOrReturnCustom(tableId, customReturnValue) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMonsterBaseTableOrReturnVoid(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return; } } while(0)

#define FetchMonsterBaseTableOrContinue(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; continue; } } while(0)

#define FetchMonsterBaseTableOrReturnFalse(tableId) \
    const auto [monsterBaseTable, fetchResult] = MonsterBaseConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( monsterBaseTable )) { LOG_ERROR << "MonsterBase table not found for ID: " << tableId; return false; } } while(0)