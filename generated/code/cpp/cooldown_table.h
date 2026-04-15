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
#include "table/proto/cooldown_table.pb.h"

class CooldownTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const CooldownTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        CooldownTableData data;
        KeyValueDataType kvData;
    };

    static CooldownTableManager& Instance() {
        static CooldownTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const CooldownTableData& FindAll() const { return snapshot_->data; }

    std::pair<const CooldownTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const CooldownTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }

    // ---- FindByIds (IN) ----

    std::vector<const CooldownTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const CooldownTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const CooldownTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const CooldownTable*> Where(const std::function<bool(const CooldownTable&)>& pred) const {
        std::vector<const CooldownTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const CooldownTable* First(const std::function<bool(const CooldownTable&)>& pred) const {
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                return &snapshot_->data.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

private:
    LoadSuccessCallback loadSuccessCallback_;
    std::unique_ptr<Snapshot> snapshot_ = std::make_unique<Snapshot>();
};

inline const CooldownTableData& FindAllCooldownTable() {
    return CooldownTableManager::Instance().FindAll();
}

#define FetchAndValidateCooldownTable(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomCooldownTable(prefix, tableId) \
    const auto [prefix##CooldownTable, prefix##fetchResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##CooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchCooldownTableOrReturnCustom(tableId, customReturnValue) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchCooldownTableOrReturnVoid(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return; } } while(0)

#define FetchCooldownTableOrContinue(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; continue; } } while(0)

#define FetchCooldownTableOrReturnFalse(tableId) \
    const auto [cooldownTable, fetchResult] = CooldownTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(cooldownTable)) { LOG_ERROR << "Cooldown table not found for ID: " << tableId; return false; } } while(0)
