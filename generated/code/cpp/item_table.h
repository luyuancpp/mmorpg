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
#include "table/proto/item_table.pb.h"

class ItemTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ItemTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        ItemTableData data;
        KeyValueDataType kvData;
    };

    static ItemTableManager& Instance() {
        static ItemTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const ItemTableData& FindAll() const { return snapshot_->data; }

    std::pair<const ItemTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const ItemTable*, uint32_t> FindByIdSilent(uint32_t tableId);
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

    std::vector<const ItemTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const ItemTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const ItemTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const ItemTable*> Where(const std::function<bool(const ItemTable&)>& pred) const {
        std::vector<const ItemTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const ItemTable* First(const std::function<bool(const ItemTable&)>& pred) const {
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
