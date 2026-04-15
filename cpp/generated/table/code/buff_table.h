#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
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

    static BuffTableManager& Instance() {
        static BuffTableManager instance;
        return instance;
    }

    const BuffTableData& All() const { return data_; }

    std::pair<const BuffTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const BuffTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    double GetHealth_regeneration(uint32_t tableId) {
        auto [table, ok] = GetTable(tableId);
        if (!ok || table == nullptr) {
            LOG_ERROR << "Health_regeneration table not found for ID: " << tableId;
            return double();
        }
        return expression_health_regeneration_.Value(table->health_regeneration());
    }
    void SetHealth_regenerationParam(const std::vector<double>& paramList) {
        expression_health_regeneration_.SetParam(paramList);
    }

    double GetBonus_damage(uint32_t tableId) {
        auto [table, ok] = GetTable(tableId);
        if (!ok || table == nullptr) {
            LOG_ERROR << "Bonus_damage table not found for ID: " << tableId;
            return double();
        }
        return expression_bonus_damage_.Value(table->bonus_damage());
    }
    void SetBonus_damageParam(const std::vector<double>& paramList) {
        expression_bonus_damage_.SetParam(paramList);
    }

    const std::unordered_multimap<double, const BuffTable*>& GetInterval_effectIndex() const { return idx_interval_effect_; }
    const std::unordered_multimap<uint32_t, const BuffTable*>& GetSub_buffIndex() const { return idx_sub_buff_; }
    const std::unordered_multimap<uint32_t, const BuffTable*>& GetTarget_sub_buffIndex() const { return idx_target_sub_buff_; }

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }
    std::size_t CountByInterval_effectIndex(double key) const { return idx_interval_effect_.count(key); }
    std::size_t CountBySub_buffIndex(uint32_t key) const { return idx_sub_buff_.count(key); }
    std::size_t CountByTarget_sub_buffIndex(uint32_t key) const { return idx_target_sub_buff_.count(key); }

    // ---- Batch Lookup (IN) ----

    std::vector<const BuffTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const BuffTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const BuffTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const BuffTable*> Filter(const std::function<bool(const BuffTable&)>& pred) const {
        std::vector<const BuffTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const BuffTable* FindFirst(const std::function<bool(const BuffTable&)>& pred) const {
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
    BuffTableData data_;
    KeyValueDataType kv_data_;
    ExcelExpression<double> expression_health_regeneration_;
    ExcelExpression<double> expression_bonus_damage_;
    std::unordered_multimap<double, const BuffTable*> idx_interval_effect_;
    std::unordered_multimap<uint32_t, const BuffTable*> idx_sub_buff_;
    std::unordered_multimap<uint32_t, const BuffTable*> idx_target_sub_buff_;
};

inline const BuffTableData& GetBuffAllTable() {
    return BuffTableManager::Instance().All();
}

#define FetchAndValidateBuffTable(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBuffTable(prefix, tableId) \
    const auto [prefix##BuffTable, prefix##fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##BuffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBuffTableOrReturnCustom(tableId, customReturnValue) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBuffTableOrReturnVoid(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return; } } while(0)

#define FetchBuffTableOrContinue(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; continue; } } while(0)

#define FetchBuffTableOrReturnFalse(tableId) \
    const auto [buffTable, fetchResult] = BuffTableManager::Instance().GetTable(tableId); \
    do { if (!(buffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return false; } } while(0)
