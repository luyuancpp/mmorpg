#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/mirror_table.pb.h"

class MirrorTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MirrorTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static MirrorTableManager& Instance() {
        static MirrorTableManager instance;
        return instance;
    }

    const MirrorTableData& All() const { return data_; }

    std::pair<const MirrorTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MirrorTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    // FK: scene_id → BaseScene.id
    // FK: main_scene_id → World.id

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }

    // ---- Batch Lookup (IN) ----

    std::vector<const MirrorTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const MirrorTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const MirrorTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const MirrorTable*> Filter(const std::function<bool(const MirrorTable&)>& pred) const {
        std::vector<const MirrorTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const MirrorTable* FindFirst(const std::function<bool(const MirrorTable&)>& pred) const {
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
    MirrorTableData data_;
    KeyValueDataType kv_data_;
};

inline const MirrorTableData& GetMirrorAllTable() {
    return MirrorTableManager::Instance().All();
}

#define FetchAndValidateMirrorTable(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMirrorTable(prefix, tableId) \
    const auto [prefix##MirrorTable, prefix##fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMirrorTableOrReturnCustom(tableId, customReturnValue) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMirrorTableOrReturnVoid(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return; } } while(0)

#define FetchMirrorTableOrContinue(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; continue; } } while(0)

#define FetchMirrorTableOrReturnFalse(tableId) \
    const auto [mirrorTable, fetchResult] = MirrorTableManager::Instance().GetTable(tableId); \
    do { if (!(mirrorTable)) { LOG_ERROR << "Mirror table not found for ID: " << tableId; return false; } } while(0)
