#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/item_table.pb.h"

class ItemTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static ItemTableManager& Instance() {
        static ItemTableManager instance;
        return instance;
    }

    const ItemTableData& FindAll() const { return data_; }

    std::pair<const ItemTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const ItemTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const ItemTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ItemTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const ItemTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const ItemTable*> Where(const std::function<bool(const ItemTable&)>& pred) const {
        std::vector<const ItemTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const ItemTable* First(const std::function<bool(const ItemTable&)>& pred) const {
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
    ItemTableData data_;
    KeyValueDataType kv_data_;
};

inline const ItemTableData& FindAllItemTable() {
    return ItemTableManager::Instance().FindAll();
}

#define FetchAndValidateItemTable(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomItemTable(prefix, tableId) \
    const auto [prefix##ItemTable, prefix##fetchResult] = ItemTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##ItemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchItemTableOrReturnCustom(tableId, customReturnValue) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchItemTableOrReturnVoid(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return; } } while(0)

#define FetchItemTableOrContinue(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; continue; } } while(0)

#define FetchItemTableOrReturnFalse(tableId) \
    const auto [itemTable, fetchResult] = ItemTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(itemTable)) { LOG_ERROR << "Item table not found for ID: " << tableId; return false; } } while(0)
