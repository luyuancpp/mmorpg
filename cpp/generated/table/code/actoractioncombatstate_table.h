#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
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

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }

    // ---- Batch Lookup (IN) ----

    std::vector<const ActorActionCombatStateTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ActorActionCombatStateTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const ActorActionCombatStateTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const ActorActionCombatStateTable*> Filter(const std::function<bool(const ActorActionCombatStateTable&)>& pred) const {
        std::vector<const ActorActionCombatStateTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const ActorActionCombatStateTable* FindFirst(const std::function<bool(const ActorActionCombatStateTable&)>& pred) const {
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                return &data_.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

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
