#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/actoractionstate_table.pb.h"

class ActorActionStateTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const ActorActionStateTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        ActorActionStateTableData data;
        IdMapType idMap;
    };

    static ActorActionStateTableManager& Instance() {
        static ActorActionStateTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const ActorActionStateTableData& FindAll() const { return snapshot->data; }

    std::pair<const ActorActionStateTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const ActorActionStateTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const ActorActionStateTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ActorActionStateTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const ActorActionStateTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const ActorActionStateTable*> Where(const std::function<bool(const ActorActionStateTable&)>& pred) const {
        std::vector<const ActorActionStateTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const ActorActionStateTable* First(const std::function<bool(const ActorActionStateTable&)>& pred) const {
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                return &snapshot->data.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

private:
    LoadSuccessCallback loadSuccessCallback;
    std::unique_ptr<Snapshot> snapshot = std::make_unique<Snapshot>();
};

inline const ActorActionStateTableData& FindAllActorActionStateTable() {
    return ActorActionStateTableManager::Instance().FindAll();
}

#define LookupActorActionState(tableId) \
    const auto [actorActionStateRow, actorActionStateResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateRow)) { LOG_ERROR << "ActorActionState row not found for ID: " << tableId; return actorActionStateResult; } } while(0)

#define LookupActorActionStateAs(prefix, tableId) \
    const auto [prefix##ActorActionStateRow, prefix##ActorActionStateResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##ActorActionStateRow)) { LOG_ERROR << "ActorActionState row not found for ID: " << tableId; return prefix##ActorActionStateResult; } } while(0)

#define LookupActorActionStateOrReturn(tableId, customReturnValue) \
    const auto [actorActionStateRow, actorActionStateResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateRow)) { LOG_ERROR << "ActorActionState row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupActorActionStateOrVoid(tableId) \
    const auto [actorActionStateRow, actorActionStateResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateRow)) { LOG_ERROR << "ActorActionState row not found for ID: " << tableId; return; } } while(0)

#define LookupActorActionStateOrContinue(tableId) \
    const auto [actorActionStateRow, actorActionStateResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateRow)) { LOG_ERROR << "ActorActionState row not found for ID: " << tableId; continue; } } while(0)

#define LookupActorActionStateOrFalse(tableId) \
    const auto [actorActionStateRow, actorActionStateResult] = ActorActionStateTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(actorActionStateRow)) { LOG_ERROR << "ActorActionState row not found for ID: " << tableId; return false; } } while(0)
