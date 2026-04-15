#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
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

    static SkillTableManager& Instance() {
        static SkillTableManager instance;
        return instance;
    }

    const SkillTableData& FindAll() const { return data_; }

    std::pair<const SkillTable*, uint32_t> FindById(uint32_t tableId);
    std::pair<const SkillTable*, uint32_t> FindByIdSilent(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

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
        return expression_damage_.Value(table->damage());
    }
    void SetDamageParam(const std::vector<double>& paramList) {
        expression_damage_.SetParam(paramList);
    }

    const std::unordered_multimap<uint32_t, const SkillTable*>& GetSkill_typeIndex() const { return idx_skill_type_; }
    const std::unordered_multimap<uint32_t, const SkillTable*>& GetTargeting_modeIndex() const { return idx_targeting_mode_; }
    const std::unordered_multimap<uint32_t, const SkillTable*>& GetEffectIndex() const { return idx_effect_; }

    // ---- Exists ----

    bool Exists(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Count ----

    std::size_t Count() const { return kv_data_.size(); }
    std::size_t CountBySkill_typeIndex(uint32_t key) const { return idx_skill_type_.count(key); }
    std::size_t CountByTargeting_modeIndex(uint32_t key) const { return idx_targeting_mode_.count(key); }
    std::size_t CountByEffectIndex(uint32_t key) const { return idx_effect_.count(key); }

    // ---- FindByIds (IN) ----

    std::vector<const SkillTable*> FindByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const SkillTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- RandOne ----

    const SkillTable* RandOne() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Where / First ----

    std::vector<const SkillTable*> Where(const std::function<bool(const SkillTable&)>& pred) const {
        std::vector<const SkillTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const SkillTable* First(const std::function<bool(const SkillTable&)>& pred) const {
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
    SkillTableData data_;
    KeyValueDataType kv_data_;
    ExcelExpression<double> expression_damage_;
    std::unordered_multimap<uint32_t, const SkillTable*> idx_skill_type_;
    std::unordered_multimap<uint32_t, const SkillTable*> idx_targeting_mode_;
    std::unordered_multimap<uint32_t, const SkillTable*> idx_effect_;
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
