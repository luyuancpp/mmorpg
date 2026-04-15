#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
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

    static BaseSceneTableManager& Instance() {
        static BaseSceneTableManager instance;
        return instance;
    }

    const BaseSceneTableData& FindAll() const { return data_; }

    std::pair<const BaseSceneTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const BaseSceneTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const BaseSceneTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const BaseSceneTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const BaseSceneTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const BaseSceneTable*> Where(const std::function<bool(const BaseSceneTable&)>& pred) const {
        std::vector<const BaseSceneTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const BaseSceneTable* First(const std::function<bool(const BaseSceneTable&)>& pred) const {
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
    BaseSceneTableData data_;
    KeyValueDataType kv_data_;
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
