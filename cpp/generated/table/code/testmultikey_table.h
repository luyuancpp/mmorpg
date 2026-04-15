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
#include "table/proto/testmultikey_table.pb.h"

class TestMultiKeyTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const TestMultiKeyTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        TestMultiKeyTableData data;
        KeyValueDataType kvData;
        std::unordered_map<std::string, const TestMultiKeyTable*> kvstring_key;
        std::unordered_map<uint32_t, const TestMultiKeyTable*> kvuint32_key;
        std::unordered_map<int32_t, const TestMultiKeyTable*> kvint32_key;
        std::unordered_multimap<std::string, const TestMultiKeyTable*> kvm_string_key;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> kvm_uint32_key;
        std::unordered_multimap<int32_t, const TestMultiKeyTable*> kvm_int32_key;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> idxeffect;
    };

    static TestMultiKeyTableManager& Instance() {
        static TestMultiKeyTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const TestMultiKeyTableData& FindAll() const { return snapshot_->data; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const TestMultiKeyTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByString_key(const std::string& key) const;
    const std::unordered_map<std::string, const TestMultiKeyTable*>& GetString_keyData() const { return snapshot_->kvstring_key; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByUint32_key(uint32_t key) const;
    const std::unordered_map<uint32_t, const TestMultiKeyTable*>& GetUint32_keyData() const { return snapshot_->kvuint32_key; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByInt32_key(int32_t key) const;
    const std::unordered_map<int32_t, const TestMultiKeyTable*>& GetInt32_keyData() const { return snapshot_->kvint32_key; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByM_string_key(const std::string& key) const;
    const std::unordered_multimap<std::string, const TestMultiKeyTable*>& GetM_string_keyData() const { return snapshot_->kvm_string_key; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByM_uint32_key(uint32_t key) const;
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetM_uint32_keyData() const { return snapshot_->kvm_uint32_key; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByM_int32_key(int32_t key) const;
    const std::unordered_multimap<int32_t, const TestMultiKeyTable*>& GetM_int32_keyData() const { return snapshot_->kvm_int32_key; }

    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetEffectIndex() const { return snapshot_->idxeffect; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }
    bool ExistsByString_key(const std::string& key) const { return snapshot_->kvstring_key.count(key) > 0; }
    bool ExistsByUint32_key(uint32_t key) const { return snapshot_->kvuint32_key.count(key) > 0; }
    bool ExistsByInt32_key(int32_t key) const { return snapshot_->kvint32_key.count(key) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountByM_string_key(const std::string& key) const { return snapshot_->kvm_string_key.count(key); }
    std::size_t CountByM_uint32_key(uint32_t key) const { return snapshot_->kvm_uint32_key.count(key); }
    std::size_t CountByM_int32_key(int32_t key) const { return snapshot_->kvm_int32_key.count(key); }
    std::size_t CountByEffectIndex(uint32_t key) const { return snapshot_->idxeffect.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const TestMultiKeyTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const TestMultiKeyTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const TestMultiKeyTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const TestMultiKeyTable*> Where(const std::function<bool(const TestMultiKeyTable&)>& pred) const {
        std::vector<const TestMultiKeyTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const TestMultiKeyTable* First(const std::function<bool(const TestMultiKeyTable&)>& pred) const {
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

inline const TestMultiKeyTableData& FindAllTestMultiKeyTable() {
    return TestMultiKeyTableManager::Instance().FindAll();
}

#define FetchAndValidateTestMultiKeyTable(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomTestMultiKeyTable(prefix, tableId) \
    const auto [prefix##TestMultiKeyTable, prefix##fetchResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##TestMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchTestMultiKeyTableOrReturnCustom(tableId, customReturnValue) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchTestMultiKeyTableOrReturnVoid(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return; } } while(0)

#define FetchTestMultiKeyTableOrContinue(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; continue; } } while(0)

#define FetchTestMultiKeyTableOrReturnFalse(tableId) \
    const auto [testMultiKeyTable, fetchResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyTable)) { LOG_ERROR << "TestMultiKey table not found for ID: " << tableId; return false; } } while(0)
