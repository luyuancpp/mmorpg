#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/actoractionstate_table.pb.h"

class ActorActionStateTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionStateTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static ActorActionStateTableManager& Instance() {
        static ActorActionStateTableManager instance;
        return instance;
    }

    const ActorActionStateTableData& FindAll() const { return data_; }

    std::pair<const ActorActionStateTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const ActorActionStateTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return kv_data_.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const ActorActionStateTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ActorActionStateTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const ActorActionStateTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const ActorActionStateTable*> Where(const std::function<bool(const ActorActionStateTable&)>& pred) const {
        std::vector<const ActorActionStateTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const ActorActionStateTable* First(const std::function<bool(const ActorActionStateTable&)>& pred) const {
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
    ActorActionStateTableData data_;
    KeyValueDataType kv_data_;
};

inline const ActorActionStateTableData& FindAllActorActionStateTable() {
    return ActorActionStateTableManager::Instance().FindAll();
}

#define FetchAndValidateActorActionStateTable(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomActorActionStateTable(prefix, tableId) \
    const auto [prefix##ActorActionStateTable, prefix##fetchResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##ActorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchActorActionStateTableOrReturnCustom(tableId, customReturnValue) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchActorActionStateTableOrReturnVoid(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return; } } while(0)

#define FetchActorActionStateTableOrContinue(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; continue; } } while(0)

#define FetchActorActionStateTableOrReturnFalse(tableId) \
    const auto [actorActionStateTable, fetchResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << tableId; return false; } } while(0)
