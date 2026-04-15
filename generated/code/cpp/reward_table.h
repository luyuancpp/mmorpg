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
#include "table/proto/reward_table.pb.h"

class RewardTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const RewardTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        RewardTableData data;
        IdMapType idMap;
    };

    static RewardTableManager& Instance() {
        static RewardTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const RewardTableData& FindAll() const { return snapshot->data; }

    std::pair<const RewardTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const RewardTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const RewardTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const RewardTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const RewardTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const RewardTable*> Where(const std::function<bool(const RewardTable&)>& pred) const {
        std::vector<const RewardTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const RewardTable* First(const std::function<bool(const RewardTable&)>& pred) const {
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

inline const RewardTableData& FindAllRewardTable() {
    return RewardTableManager::Instance().FindAll();
}

#define LookupReward(tableId) \
    const auto [rewardRow, rewardResult] = RewardTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(rewardRow)) { LOG_ERROR << "Reward row not found for ID: " << tableId; return rewardResult; } } while(0)

#define LookupRewardAs(prefix, tableId) \
    const auto [prefix##RewardRow, prefix##RewardResult] = RewardTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##RewardRow)) { LOG_ERROR << "Reward row not found for ID: " << tableId; return prefix##RewardResult; } } while(0)

#define LookupRewardOrReturn(tableId, customReturnValue) \
    const auto [rewardRow, rewardResult] = RewardTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(rewardRow)) { LOG_ERROR << "Reward row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupRewardOrVoid(tableId) \
    const auto [rewardRow, rewardResult] = RewardTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(rewardRow)) { LOG_ERROR << "Reward row not found for ID: " << tableId; return; } } while(0)

#define LookupRewardOrContinue(tableId) \
    const auto [rewardRow, rewardResult] = RewardTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(rewardRow)) { LOG_ERROR << "Reward row not found for ID: " << tableId; continue; } } while(0)

#define LookupRewardOrFalse(tableId) \
    const auto [rewardRow, rewardResult] = RewardTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(rewardRow)) { LOG_ERROR << "Reward row not found for ID: " << tableId; return false; } } while(0)
