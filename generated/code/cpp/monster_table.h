
#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/monster_table.pb.h"

class MonsterTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MonsterTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static MonsterTableManager& Instance() {
        static MonsterTableManager instance;
        return instance;
    }

    const MonsterTableData& All() const { return data_; }

    std::pair<const MonsterTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MonsterTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }








private:
    LoadSuccessCallback loadSuccessCallback_;
    MonsterTableData data_;
    KeyValueDataType kv_data_;



};

inline const MonsterTableData& GetMonsterAllTable() {
    return MonsterTableManager::Instance().All();
}

#define FetchAndValidateMonsterTable(tableId) \
    const auto [monsterTable, fetchResult] = MonsterTableManager::Instance().GetTable(tableId); \
    do { if (!(monsterTable)) { LOG_ERROR << "Monster table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMonsterTable(prefix, tableId) \
    const auto [prefix##MonsterTable, prefix##fetchResult] = MonsterTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MonsterTable)) { LOG_ERROR << "Monster table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMonsterTableOrReturnCustom(tableId, customReturnValue) \
    const auto [monsterTable, fetchResult] = MonsterTableManager::Instance().GetTable(tableId); \
    do { if (!(monsterTable)) { LOG_ERROR << "Monster table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMonsterTableOrReturnVoid(tableId) \
    const auto [monsterTable, fetchResult] = MonsterTableManager::Instance().GetTable(tableId); \
    do { if (!(monsterTable)) { LOG_ERROR << "Monster table not found for ID: " << tableId; return; } } while(0)

#define FetchMonsterTableOrContinue(tableId) \
    const auto [monsterTable, fetchResult] = MonsterTableManager::Instance().GetTable(tableId); \
    do { if (!(monsterTable)) { LOG_ERROR << "Monster table not found for ID: " << tableId; continue; } } while(0)

#define FetchMonsterTableOrReturnFalse(tableId) \
    const auto [monsterTable, fetchResult] = MonsterTableManager::Instance().GetTable(tableId); \
    do { if (!(monsterTable)) { LOG_ERROR << "Monster table not found for ID: " << tableId; return false; } } while(0)