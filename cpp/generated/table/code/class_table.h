#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/class_table.pb.h"

class ClassTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ClassTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static ClassTableManager& Instance() {
        static ClassTableManager instance;
        return instance;
    }

    const ClassTableData& All() const { return data_; }

    std::pair<const ClassTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ClassTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const ClassTable*>& GetSkillIndex() const { return idx_skill_; }

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }
    std::size_t CountBySkillIndex(uint32_t key) const { return idx_skill_.count(key); }

    // ---- Batch Lookup (IN) ----

    std::vector<const ClassTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ClassTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const ClassTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const ClassTable*> Filter(const std::function<bool(const ClassTable&)>& pred) const {
        std::vector<const ClassTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const ClassTable* FindFirst(const std::function<bool(const ClassTable&)>& pred) const {
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
    ClassTableData data_;
    KeyValueDataType kv_data_;
    std::unordered_multimap<uint32_t, const ClassTable*> idx_skill_;
};

inline const ClassTableData& GetClassAllTable() {
    return ClassTableManager::Instance().All();
}

#define FetchAndValidateClassTable(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomClassTable(prefix, tableId) \
    const auto [prefix##ClassTable, prefix##fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##ClassTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchClassTableOrReturnCustom(tableId, customReturnValue) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchClassTableOrReturnVoid(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return; } } while(0)

#define FetchClassTableOrContinue(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; continue; } } while(0)

#define FetchClassTableOrReturnFalse(tableId) \
    const auto [classTable, fetchResult] = ClassTableManager::Instance().GetTable(tableId); \
    do { if (!(classTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return false; } } while(0)
