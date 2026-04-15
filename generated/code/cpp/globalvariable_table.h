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
#include "table/proto/globalvariable_table.pb.h"

class GlobalVariableTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const GlobalVariableTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        GlobalVariableTableData data;
        KeyValueDataType kvData;
    };

    static GlobalVariableTableManager& Instance() {
        static GlobalVariableTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const GlobalVariableTableData& FindAll() const { return snapshot_->data; }

    std::pair<const GlobalVariableTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const GlobalVariableTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const GlobalVariableTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const GlobalVariableTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const GlobalVariableTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const GlobalVariableTable*> Where(const std::function<bool(const GlobalVariableTable&)>& pred) const {
        std::vector<const GlobalVariableTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const GlobalVariableTable* First(const std::function<bool(const GlobalVariableTable&)>& pred) const {
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

inline const GlobalVariableTableData& FindAllGlobalVariableTable() {
    return GlobalVariableTableManager::Instance().FindAll();
}

#define FetchAndValidateGlobalVariableTable(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomGlobalVariableTable(prefix, tableId) \
    const auto [prefix##GlobalVariableTable, prefix##fetchResult] = GlobalVariableTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##GlobalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchGlobalVariableTableOrReturnCustom(tableId, customReturnValue) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchGlobalVariableTableOrReturnVoid(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return; } } while(0)

#define FetchGlobalVariableTableOrContinue(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; continue; } } while(0)

#define FetchGlobalVariableTableOrReturnFalse(tableId) \
    const auto [globalVariableTable, fetchResult] = GlobalVariableTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(globalVariableTable)) { LOG_ERROR << "GlobalVariable table not found for ID: " << tableId; return false; } } while(0)
