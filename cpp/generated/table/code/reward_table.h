#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/reward_table.pb.h"

class RewardTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const RewardTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static RewardTableManager& Instance() {
        static RewardTableManager instance;
        return instance;
    }

    const RewardTableData& FindAll() const { return data_; }

    std::pair<const RewardTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const RewardTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const RewardTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const RewardTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const RewardTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const RewardTable*> Where(const std::function<bool(const RewardTable&)>& pred) const {
        std::vector<const RewardTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const RewardTable* First(const std::function<bool(const RewardTable&)>& pred) const {
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
    RewardTableData data_;
    KeyValueDataType kv_data_;
};

inline const RewardTableData& FindAllRewardTable() {
    return RewardTableManager::Instance().FindAll();
}

#define FetchAndValidateRewardTable(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().FindById(tableId); \
    do { if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomRewardTable(prefix, tableId) \
    const auto [prefix##RewardTable, prefix##fetchResult] = RewardTableManager::Instance().FindById(tableId); \
    do { if (!(prefix##RewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchRewardTableOrReturnCustom(tableId, customReturnValue) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().FindById(tableId); \
    do { if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchRewardTableOrReturnVoid(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().FindById(tableId); \
    do { if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; return; } } while(0)

#define FetchRewardTableOrContinue(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().FindById(tableId); \
    do { if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; continue; } } while(0)

#define FetchRewardTableOrReturnFalse(tableId) \
    const auto [rewardTable, fetchResult] = RewardTableManager::Instance().FindById(tableId); \
    do { if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId; return false; } } while(0)
