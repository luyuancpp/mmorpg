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
#include "table/proto/skill_table.pb.h"

class SkillTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const SkillTable*>;
    using LoadSuccessCallback = std::function<void()>;

    // Internal snapshot holding all parsed data and indices.
    // Load() builds a new snapshot and swaps it in, replacing the old one.
    struct Snapshot {
        SkillTableData data;
        KeyValueDataType kvData;
        ExcelExpression<double> expressiondamage;
        std::unordered_multimap<uint32_t, const SkillTable*> idxskill_type;
        std::unordered_multimap<uint32_t, const SkillTable*> idxtargeting_mode;
        std::unordered_multimap<uint32_t, const SkillTable*> idxeffect;
    };

    static SkillTableManager& Instance() {
        static SkillTableManager instance;
        return instance;
    }

    const Snapshot& GetSnapshot() const { return *snapshot_; }

    const SkillTableData& FindAll() const { return snapshot_->data; }

    std::pair<const SkillTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const SkillTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return snapshot_->kvData; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

    double GetDamage(uint32_t tableId) {
        auto [table, fetchResult] = FindById(tableId);
        if (table == nullptr) {
            return double();
        }
        return snapshot_->expressiondamage.Value(table->damage());
    }
    void SetDamageParam(const std::vector<double>& paramList) {
        snapshot_->expressiondamage.SetParam(paramList);
    }

    const std::unordered_multimap<uint32_t, const SkillTable*>& GetSkill_typeIndex() const { return snapshot_->idxskill_type; }
    const std::unordered_multimap<uint32_t, const SkillTable*>& GetTargeting_modeIndex() const { return snapshot_->idxtargeting_mode; }
    const std::unordered_multimap<uint32_t, const SkillTable*>& GetEffectIndex() const { return snapshot_->idxeffect; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return snapshot_->kvData.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return snapshot_->kvData.size(); }
    std::size_t CountBySkill_typeIndex(uint32_t key) const { return snapshot_->idxskill_type.count(key); }
    std::size_t CountByTargeting_modeIndex(uint32_t key) const { return snapshot_->idxtargeting_mode.count(key); }
    std::size_t CountByEffectIndex(uint32_t key) const { return snapshot_->idxeffect.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const SkillTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const SkillTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = snapshot_->kvData.find(id); it != snapshot_->kvData.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const SkillTable* RandOne() const {
        if (snapshot_->data.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, snapshot_->data.data_size() - 1);
        return &snapshot_->data.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const SkillTable*> Where(const std::function<bool(const SkillTable&)>& pred) const {
        std::vector<const SkillTable*> result;
        for (int i = 0; i < snapshot_->data.data_size(); ++i) {
            if (pred(snapshot_->data.data(i))) {
                result.push_back(&snapshot_->data.data(i));
            }
        }
        return result;
    }

    const SkillTable* First(const std::function<bool(const SkillTable&)>& pred) const {
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

inline const SkillTableData& FindAllSkillTable() {
    return SkillTableManager::Instance().FindAll();
}

#define FetchAndValidateSkillTable(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomSkillTable(prefix, tableId) \
    const auto [prefix##SkillTable, prefix##fetchResult] = SkillTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(prefix##SkillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchSkillTableOrReturnCustom(tableId, customReturnValue) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchSkillTableOrReturnVoid(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; return; } } while(0)

#define FetchSkillTableOrContinue(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; continue; } } while(0)

#define FetchSkillTableOrReturnFalse(tableId) \
    const auto [skillTable, fetchResult] = SkillTableManager::Instance().FindByIdSilent(tableId); \
    do { if (!(skillTable)) { LOG_ERROR << "Skill table not found for ID: " << tableId; return false; } } while(0)
