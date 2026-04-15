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
#include "table/proto/condition_table.pb.h"

class ConditionTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ConditionTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        ConditionTableData data;
        KeyValueDataType kvData;
        std::unordered_multimap<uint32_t, const ConditionTable*> idxcondition1;
        std::unordered_multimap<uint32_t, const ConditionTable*> idxcondition2;
        std::unordered_multimap<uint32_t, const ConditionTable*> idxcondition3;
        std::unordered_multimap<uint32_t, const ConditionTable*> idxcondition4;
    };

    static ConditionTableManager& Instance() {
        static ConditionTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const ConditionTableData& FindAll() const { return snapshot_->data; }

    std::pair<const ConditionTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const ConditionTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const ConditionTable*>& GetCondition1Index() const { return snapshot_->idxcondition1; }
    const std::unordered_multimap<uint32_t, const ConditionTable*>& GetCondition2Index() const { return snapshot_->idxcondition2; }
    const std::unordered_multimap<uint32_t, const ConditionTable*>& GetCondition3Index() const { return snapshot_->idxcondition3; }
    const std::unordered_multimap<uint32_t, const ConditionTable*>& GetCondition4Index() const { return snapshot_->idxcondition4; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountByCondition1Index(uint32_t key) const { return snapshot_->idxcondition1.count(key); }
    std::size_t CountByCondition2Index(uint32_t key) const { return snapshot_->idxcondition2.count(key); }
    std::size_t CountByCondition3Index(uint32_t key) const { return snapshot_->idxcondition3.count(key); }
    std::size_t CountByCondition4Index(uint32_t key) const { return snapshot_->idxcondition4.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const ConditionTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ConditionTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const ConditionTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const ConditionTable*> Where(const std::function<bool(const ConditionTable&)>& pred) const {
        std::vector<const ConditionTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const ConditionTable* First(const std::function<bool(const ConditionTable&)>& pred) const {
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

inline const ConditionTableData& FindAllConditionTable() {
    return ConditionTableManager::Instance().FindAll();
}

#define FetchAndValidateConditionTable(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomConditionTable(prefix, tableId) \
    const auto [prefix##ConditionTable, prefix##fetchResult] = ConditionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##ConditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchConditionTableOrReturnCustom(tableId, customReturnValue) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchConditionTableOrReturnVoid(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return; } } while(0)

#define FetchConditionTableOrContinue(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; continue; } } while(0)

#define FetchConditionTableOrReturnFalse(tableId) \
    const auto [conditionTable, fetchResult] = ConditionTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(conditionTable)) { LOG_ERROR << "Condition table not found for ID: " << tableId; return false; } } while(0)
