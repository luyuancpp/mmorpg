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
#include "table/proto/test_table.pb.h"

class TestTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        TestTableData data;
        KeyValueDataType kvData;
        std::unordered_multimap<uint32_t, const TestTable*> idxeffect;
    };

    static TestTableManager& Instance() {
        static TestTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const TestTableData& FindAll() const { return snapshot_->data; }

    std::pair<const TestTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const TestTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const TestTable*>& GetEffectIndex() const { return snapshot_->idxeffect; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountByEffectIndex(uint32_t key) const { return snapshot_->idxeffect.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const TestTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const TestTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const TestTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const TestTable*> Where(const std::function<bool(const TestTable&)>& pred) const {
        std::vector<const TestTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const TestTable* First(const std::function<bool(const TestTable&)>& pred) const {
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

inline const TestTableData& FindAllTestTable() {
    return TestTableManager::Instance().FindAll();
}

#define FetchAndValidateTestTable(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestTable(prefix, tableId) \
    const auto [prefix##TestTable, prefix##fetchResult] = TestTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##TestTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestTableOrReturnVoid(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return; } } while(0)

#define FetchTestTableOrContinue(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestTableOrReturnFalse(tableId) \
    const auto [testTable, fetchResult] = TestTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testTable)) { LOG_ERROR << "Test table not found for ID: " << tableId; return false; } } while(0)
