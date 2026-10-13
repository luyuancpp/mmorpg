#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>
#include "table_log.h"
#include "table/proto/equipgemsocket_table.pb.h"

class EquipGemSocketTableManager {
public:
    using IdMapType = std::unordered_map<uint32_t, const EquipGemSocketTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        EquipGemSocketTableData data;
        IdMapType idMap;
        std::unordered_map<uint32_t, std::vector<const EquipGemSocketTable*>> equipKindIndex;
    };

    static EquipGemSocketTableManager& Instance() {
        static EquipGemSocketTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot; }

    const EquipGemSocketTableData& FindAll() const { return snapshot->data; }

    // ---- 生命周期契约(热更) ----
    //
    // Load() 会整批建好新 Snapshot 再换掉旧的,**旧 Snapshot 当场析构**。
    // 所以下面所有返回指针 / 引用 / 迭代器的接口,返回值只在**下一次 Load() 之前**有效。
    //
    //   ✅ 存 id,用的时候现查
    //   ❌ 把 const EquipGemSocketTable* 存进成员、容器、闭包、协程帧
    //
    // 存指针在热更那一刻就是野指针,而且不会有任何报错。
    std::pair<const EquipGemSocketTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const EquipGemSocketTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const IdMapType& GetIdMap() const { return snapshot->idMap; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback) { loadSuccessCallback(); } }

    const std::unordered_map<uint32_t, std::vector<const EquipGemSocketTable*>>& GetEquipKindIndex() const { return snapshot->equipKindIndex; }
    const std::vector<const EquipGemSocketTable*>& GetByEquipKind(uint32_t key) const {
        static const std::vector<const EquipGemSocketTable*> kEmpty;
        auto it = snapshot->equipKindIndex.find(key);
        return it != snapshot->equipKindIndex.end() ? it->second : kEmpty;
    }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot->idMap.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot->idMap.size(); }
    std::size_t CountByEquipKindIndex(uint32_t key) const {
        auto it = snapshot->equipKindIndex.find(key);
        return it != snapshot->equipKindIndex.end() ? it->second.size() : 0;
    }

    // ---- FindByIds (IN) ----

    std::vector<const EquipGemSocketTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const EquipGemSocketTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot->idMap.find(id); it != snapshot->idMap.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const EquipGemSocketTable* RandOne() const {
        if (snapshot->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot->data.data_size() - 1);
        return &snapshot->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const EquipGemSocketTable*> Where(const std::function<bool(const EquipGemSocketTable&)>& pred) const {
        std::vector<const EquipGemSocketTable*> result;
        for (int i = 0; i < snapshot->data.data_size(); ++i) {
            if (pred(snapshot->data.data(i))) {
                result.push_back(&snapshot->data.data(i));
            }
        }
        return result;
    }

    const EquipGemSocketTable* First(const std::function<bool(const EquipGemSocketTable&)>& pred) const {
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

inline const EquipGemSocketTableData& FindAllEquipGemSocketTable() {
    return EquipGemSocketTableManager::Instance().FindAll();
}

// ---- Lookup guard macros ----
// Each macro looks up a row by tableId and, on success, injects two locals into
// the current scope:
//   equipGemSocketRow    -> const EquipGemSocketTable* (the matched row)
//   equipGemSocketResult -> uint32_t status (kInvalidTableId on miss)
// On a miss they log an error (via TableLookupLogMissing in table_log.h — the macros
// deliberately do NOT stream through muduo, so this header owes muduo nothing) and
// bail out; the suffix spells out HOW they bail:
//   OrReturnError -> return the kInvalidTableId status code
//   OrReturn      -> return a caller-supplied value
//   OrReturnVoid  -> return; (for void functions)
//   OrReturnFalse -> return false;
//   OrContinue    -> continue; (skip to the next loop iteration)

#define LookupEquipGemSocketOrReturnError(tableId) \
    const auto [equipGemSocketRow, equipGemSocketResult] = EquipGemSocketTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(equipGemSocketRow)) { TableLookupLogMissing("EquipGemSocket", tableId, __FILE__, __LINE__); return equipGemSocketResult; } } while(0)

#define LookupEquipGemSocketAsOrReturnError(prefix, tableId) \
    const auto [prefix##EquipGemSocketRow, prefix##EquipGemSocketResult] = EquipGemSocketTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##EquipGemSocketRow)) { TableLookupLogMissing("EquipGemSocket", tableId, __FILE__, __LINE__); return prefix##EquipGemSocketResult; } } while(0)

#define LookupEquipGemSocketOrReturn(tableId, customReturnValue) \
    const auto [equipGemSocketRow, equipGemSocketResult] = EquipGemSocketTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(equipGemSocketRow)) { TableLookupLogMissing("EquipGemSocket", tableId, __FILE__, __LINE__); return customReturnValue; } } while(0)

#define LookupEquipGemSocketOrReturnVoid(tableId) \
    const auto [equipGemSocketRow, equipGemSocketResult] = EquipGemSocketTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(equipGemSocketRow)) { TableLookupLogMissing("EquipGemSocket", tableId, __FILE__, __LINE__); return; } } while(0)

// OrContinue 是唯一不能套 do{...}while(0) 的一个:continue 会绑定到 do-while 自身,
// 跳去求值 while(0) 判假退出,于是控制流直接落到宏后面 —— 守卫变成空操作,
// 调用方紧接着解引用空行指针。(return 系列不受影响,return 能穿透 do-while。)
// 少了这层包装也没有代价:宏本身要声明两个变量,从来就不能当单语句塞进无括号 if。
#define LookupEquipGemSocketOrContinue(tableId) \
    const auto [equipGemSocketRow, equipGemSocketResult] = EquipGemSocketTableManager::Instance().FindByIdSilent(tableId); \
    if (!(equipGemSocketRow)) { TableLookupLogMissing("EquipGemSocket", tableId, __FILE__, __LINE__); continue; }

#define LookupEquipGemSocketOrReturnFalse(tableId) \
    const auto [equipGemSocketRow, equipGemSocketResult] = EquipGemSocketTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(equipGemSocketRow)) { TableLookupLogMissing("EquipGemSocket", tableId, __FILE__, __LINE__); return false; } } while(0)
