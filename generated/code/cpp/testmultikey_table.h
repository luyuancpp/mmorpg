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
    using IdMapType = std::unordered_map<uint32_t, const TestMultiKeyTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        TestMultiKeyTableData data;
        IdMapType idMap;
        std::unordered_map<std::string, const TestMultiKeyTable*> stringKeyMap;
        std::unordered_map<uint32_t, const TestMultiKeyTable*> uint32KeyMap;
        std::unordered_map<int32_t, const TestMultiKeyTable*> int32KeyMap;
        std::unordered_multimap<std::string, const TestMultiKeyTable*> mStringKeyMap;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> mUint32KeyMap;
        std::unordered_multimap<int32_t, const TestMultiKeyTable*> mInt32KeyMap;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> effectIndex;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> testRefsIndex;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> levelIndex;
        std::unordered_multimap<uint32_t, const TestMultiKeyTable*> testRefIndex;
    };

    static TestMultiKeyTableManager& Instance() {
        static TestMultiKeyTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const TestMultiKeyTableData& FindAll() const { return snapshot->data; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const TestMultiKeyTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByStringKey(const std::string& key) const;
    const std::unordered_map<std::string, const TestMultiKeyTable*>& GetStringKeyMap() const { return snapshot->stringKeyMap; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByUint32Key(uint32_t key) const;
    const std::unordered_map<uint32_t, const TestMultiKeyTable*>& GetUint32KeyMap() const { return snapshot->uint32KeyMap; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByInt32Key(int32_t key) const;
    const std::unordered_map<int32_t, const TestMultiKeyTable*>& GetInt32KeyMap() const { return snapshot->int32KeyMap; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByMStringKey(const std::string& key) const;
    const std::unordered_multimap<std::string, const TestMultiKeyTable*>& GetMStringKeyMap() const { return snapshot->mStringKeyMap; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByMUint32Key(uint32_t key) const;
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetMUint32KeyMap() const { return snapshot->mUint32KeyMap; }

    std::pair<const TestMultiKeyTable*, uint32_t> FindByMInt32Key(int32_t key) const;
    const std::unordered_multimap<int32_t, const TestMultiKeyTable*>& GetMInt32KeyMap() const { return snapshot->mInt32KeyMap; }

    // FK: test_ref -> Test.id
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetEffectIndex() const { return snapshot->effectIndex; }
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetTestRefsIndex() const { return snapshot->testRefsIndex; }
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetLevelIndex() const { return snapshot->levelIndex; }
    std::vector<const TestMultiKeyTable*> GetByLevel(uint32_t key) const {
        auto range = snapshot->levelIndex.equal_range(key);
        std::vector<const TestMultiKeyTable*> result;
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        return result;
    }
    const std::unordered_multimap<uint32_t, const TestMultiKeyTable*>& GetTestRefIndex() const { return snapshot->testRefIndex; }
    std::vector<const TestMultiKeyTable*> GetByTestRef(uint32_t key) const {
        auto range = snapshot->testRefIndex.equal_range(key);
        std::vector<const TestMultiKeyTable*> result;
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        return result;
    }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }
    bool ExistsByStringKey(const std::string& key) const { return snapshot->stringKeyMap.count(key) > 0; }
    bool ExistsByUint32Key(uint32_t key) const { return snapshot->uint32KeyMap.count(key) > 0; }
    bool ExistsByInt32Key(int32_t key) const { return snapshot->int32KeyMap.count(key) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }
    std::size_t CountByMStringKey(const std::string& key) const { return snapshot->mStringKeyMap.count(key); }
    std::size_t CountByMUint32Key(uint32_t key) const { return snapshot->mUint32KeyMap.count(key); }
    std::size_t CountByMInt32Key(int32_t key) const { return snapshot->mInt32KeyMap.count(key); }
    std::size_t CountByEffectIndex(uint32_t key) const { return snapshot->effectIndex.count(key); }
    std::size_t CountByTestRefsIndex(uint32_t key) const { return snapshot->testRefsIndex.count(key); }
    std::size_t CountByLevelIndex(uint32_t key) const { return snapshot->levelIndex.count(key); }
    std::size_t CountByTestRefIndex(uint32_t key) const { return snapshot->testRefIndex.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const TestMultiKeyTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const TestMultiKeyTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const TestMultiKeyTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const TestMultiKeyTable*> Where(const std::function<bool(const TestMultiKeyTable&)>& pred) const {
        std::vector<const TestMultiKeyTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const TestMultiKeyTable* First(const std::function<bool(const TestMultiKeyTable&)>& pred) const {
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                return &snapshot->data.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

private:
    LoadSuccessCallback loadSuccessCallback;
    std::unique_ptr<Snapshot> snapshot = std::make_unique<Snapshot>();
};

inline const TestMultiKeyTableData& FindAllTestMultiKeyTable() {
    return TestMultiKeyTableManager::Instance().FindAll();
}

#define LookupTestMultiKey(tableId) \
    const auto [testMultiKeyRow, testMultiKeyResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyRow)) { LOG_ERROR << "TestMultiKey row not found for ID: " << tableId; return testMultiKeyResult; } } while(0)

#define LookupTestMultiKeyAs(prefix, tableId) \
    const auto [prefix##TestMultiKeyRow, prefix##TestMultiKeyResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##TestMultiKeyRow)) { LOG_ERROR << "TestMultiKey row not found for ID: " << tableId; return prefix##TestMultiKeyResult; } } while(0)

#define LookupTestMultiKeyOrReturn(tableId, customReturnValue) \
    const auto [testMultiKeyRow, testMultiKeyResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyRow)) { LOG_ERROR << "TestMultiKey row not found for ID: " << tableId; return customReturnValue; } } while(0)

#define LookupTestMultiKeyOrVoid(tableId) \
    const auto [testMultiKeyRow, testMultiKeyResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyRow)) { LOG_ERROR << "TestMultiKey row not found for ID: " << tableId; return; } } while(0)

#define LookupTestMultiKeyOrContinue(tableId) \
    const auto [testMultiKeyRow, testMultiKeyResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyRow)) { LOG_ERROR << "TestMultiKey row not found for ID: " << tableId; continue; } } while(0)

#define LookupTestMultiKeyOrFalse(tableId) \
    const auto [testMultiKeyRow, testMultiKeyResult] = TestMultiKeyTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(testMultiKeyRow)) { LOG_ERROR << "TestMultiKey row not found for ID: " << tableId; return false; } } while(0)
