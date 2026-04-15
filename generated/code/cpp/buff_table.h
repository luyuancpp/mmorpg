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
#include "table/proto/buff_table.pb.h"

class BuffTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const BuffTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        BuffTableData data;
        KeyValueDataType kvData;
        ExcelExpression<double> expressionhealth_regeneration;
        ExcelExpression<double> expressionbonus_damage;
        std::unordered_multimap<double, const BuffTable*> idxinterval_effect;
        std::unordered_multimap<uint32_t, const BuffTable*> idxsub_buff;
        std::unordered_multimap<uint32_t, const BuffTable*> idxtarget_sub_buff;
    };

    static BuffTableManager& Instance() {
        static BuffTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const BuffTableData& FindAll() const { return snapshot_->data; }

    std::pair<const BuffTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const BuffTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    double GetHealth_regeneration(uint32_t tableId) {
        auto [table, fetchResult] = FindById(tableId);
        if (table == nullptr) {
            return double();
        }
        return snapshot_->expressionhealth_regeneration.Value(table->health_regeneration());
    }
    void SetHealth_regenerationParam(const std::vector<double>& paramList) {
        snapshot_->expressionhealth_regeneration.SetParam(paramList);
    }

    double GetBonus_damage(uint32_t tableId) {
        auto [table, fetchResult] = FindById(tableId);
        if (table == nullptr) {
            return double();
        }
        return snapshot_->expressionbonus_damage.Value(table->bonus_damage());
    }
    void SetBonus_damageParam(const std::vector<double>& paramList) {
        snapshot_->expressionbonus_damage.SetParam(paramList);
    }

    const std::unordered_multimap<double, const BuffTable*>& GetInterval_effectIndex() const { return snapshot_->idxinterval_effect; }
    const std::unordered_multimap<uint32_t, const BuffTable*>& GetSub_buffIndex() const { return snapshot_->idxsub_buff; }
    const std::unordered_multimap<uint32_t, const BuffTable*>& GetTarget_sub_buffIndex() const { return snapshot_->idxtarget_sub_buff; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountByInterval_effectIndex(double key) const { return snapshot_->idxinterval_effect.count(key); }
    std::size_t CountBySub_buffIndex(uint32_t key) const { return snapshot_->idxsub_buff.count(key); }
    std::size_t CountByTarget_sub_buffIndex(uint32_t key) const { return snapshot_->idxtarget_sub_buff.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const BuffTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const BuffTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const BuffTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const BuffTable*> Where(const std::function<bool(const BuffTable&)>& pred) const {
        std::vector<const BuffTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const BuffTable* First(const std::function<bool(const BuffTable&)>& pred) const {
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

inline const BuffTableData& FindAllBuffTable() {
    return BuffTableManager::Instance().FindAll();
}

#define FetchAndValidateBuffTable(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBuffTable(prefix, tableId) \
    const auto [prefix##BuffTable, prefix##fetchResult] = BuffTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##BuffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBuffTableOrReturnCustom(tableId, customReturnValue) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBuffTableOrReturnVoid(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return; } } while(0)

#define FetchBuffTableOrContinue(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; continue; } } while(0)

#define FetchBuffTableOrReturnFalse(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return false; } } while(0)
