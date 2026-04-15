#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
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

    static TestTableManager& Instance() {
        static TestTableManager instance;
        return instance;
    }

    const TestTableData& FindAll() const { return data_; }

    std::pair<const TestTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const TestTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    const std::unordered_multimap<uint32_t, const TestTable*>& GetEffectIndex() const { return idx_effect_; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return kv_data_.size(); }
    std::size_t CountByEffectIndex(uint32_t key) const { return idx_effect_.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const TestTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const TestTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const TestTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const TestTable*> Where(const std::function<bool(const TestTable&)>& pred) const {
        std::vector<const TestTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const TestTable* First(const std::function<bool(const TestTable&)>& pred) const {
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
    TestTableData data_;
    KeyValueDataType kv_data_;
    std::unordered_multimap<uint32_t, const TestTable*> idx_effect_;
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
