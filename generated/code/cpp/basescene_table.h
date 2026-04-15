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
#include "table/proto/basescene_table.pb.h"

class BaseSceneTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const BaseSceneTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        BaseSceneTableData data;
        KeyValueDataType kvData;
    };

    static BaseSceneTableManager& Instance() {
        static BaseSceneTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const BaseSceneTableData& FindAll() const { return snapshot_->data; }

    std::pair<const BaseSceneTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const BaseSceneTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const BaseSceneTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const BaseSceneTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const BaseSceneTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const BaseSceneTable*> Where(const std::function<bool(const BaseSceneTable&)>& pred) const {
        std::vector<const BaseSceneTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const BaseSceneTable* First(const std::function<bool(const BaseSceneTable&)>& pred) const {
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

inline const BaseSceneTableData& FindAllBaseSceneTable() {
    return BaseSceneTableManager::Instance().FindAll();
}

#define FetchAndValidateBaseSceneTable(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBaseSceneTable(prefix, tableId) \
    const auto [prefix##BaseSceneTable, prefix##fetchResult] = BaseSceneTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##BaseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBaseSceneTableOrReturnCustom(tableId, customReturnValue) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBaseSceneTableOrReturnVoid(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return; } } while(0)

#define FetchBaseSceneTableOrContinue(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; continue; } } while(0)

#define FetchBaseSceneTableOrReturnFalse(tableId) \
    const auto [baseSceneTable, fetchResult] = BaseSceneTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(baseSceneTable)) { LOG_ERROR << "BaseScene table not found for ID: " << tableId; return false; } } while(0)
