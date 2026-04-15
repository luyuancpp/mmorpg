#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/globalvariable_table.pb.h"

class GlobalVariableTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static GlobalVariableTableManager& Instance() {
        static GlobalVariableTableManager instance;
        return instance;
    }

    const GlobalVariableTableData& All() const { return data_; }

    std::pair<const GlobalVariableTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const GlobalVariableTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
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

    std::vector<const GlobalVariableTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const GlobalVariableTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const GlobalVariableTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const GlobalVariableTable*> Filter(const std::function<bool(const GlobalVariableTable&)>& pred) const {
        std::vector<const GlobalVariableTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const GlobalVariableTable* FindFirst(const std::function<bool(const GlobalVariableTable&)>& pred) const {
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
    GlobalVariableTableData data_;
    KeyValueDataType kv_data_;
};

inline const GlobalVariableTableData& GetGlobalVariableAllTable() {
    return GlobalVariableTableManager::Instance().All();
}

#define FetchAndValidateGlobalVariableTable(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomGlobalVariableTable(prefix, tableId) \
    const auto [prefix##GlobalVariableTable, prefix##fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##GlobalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchGlobalVariableTableOrReturnCustom(tableId, customReturnValue) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchGlobalVariableTableOrReturnVoid(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return; } } while(0)

#define FetchGlobalVariableTableOrContinue(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; continue; } } while(0)

#define FetchGlobalVariableTableOrReturnFalse(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().GetTable(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return false; } } while(0)
