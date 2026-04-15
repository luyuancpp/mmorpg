#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/cooldown_table.pb.h"

class CooldownTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const CooldownTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static CooldownTableManager& Instance() {
        static CooldownTableManager instance;
        return instance;
    }

    const CooldownTableData& All() const { return data_; }

    std::pair<const CooldownTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const CooldownTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
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

    std::vector<const CooldownTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const CooldownTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const CooldownTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const CooldownTable*> Filter(const std::function<bool(const CooldownTable&)>& pred) const {
        std::vector<const CooldownTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const CooldownTable* FindFirst(const std::function<bool(const CooldownTable&)>& pred) const {
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
    CooldownTableData data_;
    KeyValueDataType kv_data_;
};

inline const CooldownTableData& GetCooldownAllTable() {
    return CooldownTableManager::Instance().All();
}

#define FetchAndValidateCooldownTable(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomCooldownTable(prefix, tableId) \
    const auto [prefix##CooldownTable, prefix##fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##CooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchCooldownTableOrReturnCustom(tableId, customReturnValue) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchCooldownTableOrReturnVoid(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return; } } while(0)

#define FetchCooldownTableOrContinue(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; continue; } } while(0)

#define FetchCooldownTableOrReturnFalse(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().GetTable(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return false; } } while(0)
