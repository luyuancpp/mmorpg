#pragma once

#include <string>
#include <unordered_map>

#include "proto/common/component/currency_comp.pb.h"
#include "engine/core/utils/proto/unknown_field_carry.h"

// Per-player deferred-clawback debt record (runtime mirror of proto CurrencyDebtEntry).
struct CurrencyDebt
{
    uint64_t owed{0};
    uint64_t paid{0};
    bool frozen{false};    // GM freeze — pauses auto-deduction
    uint64_t expiresAt{0}; // 0 = never expires
    std::string reason;
    std::string gmOperator;
    uint64_t createdAt{0};
    // 本条欠款里本版本不认识的字段(新版本给 CurrencyDebtEntry 加的),随这条欠款携带、存盘时原样写回;
    // 欠款被删除后重建得到的是新对象,这里为空 —— 不会把旧欠款的新字段复活到新欠款上
    // (docs/design/mixed-version-persist-field-protection.md §4.2 L2)。全网同版本时为空串。
    std::string unknownFields;

    [[nodiscard]] uint64_t Remaining() const { return owed > paid ? (owed - paid) : 0; }
};

// Runtime-only metadata. Persistent balances are stored in proto CurrencyComp.
struct PlayerCurrencyComp
{
    // Deferred-clawback debts (补缴), keyed by CurrencyType.
    std::unordered_map<uint32_t, CurrencyDebt> debts;

    bool dirty{false};

    // ── Serialization helpers (proto ⇄ runtime) ─────────────────────────

    // Load debts from the persisted CurrencyComp.debts repeated field.
    void LoadFromProto(const CurrencyComp &proto)
    {
        debts.clear();
        for (const auto &entry : proto.debts())
        {
            CurrencyDebt d;
            d.owed = entry.owed();
            d.paid = entry.paid();
            d.frozen = entry.frozen();
            d.expiresAt = entry.expires_at();
            d.reason = entry.reason();
            d.gmOperator = entry.gm_operator();
            d.createdAt = entry.created_at();
            d.unknownFields = unknown_field_carry::Capture(entry);
            debts[entry.currency_type()] = std::move(d);
        }
    }

    // Save runtime debts back into CurrencyComp.debts for persistence.
    // 返回 false = 某条欠款携带的未知字段解析失败(只可能是内存被破坏),调用方必须拒绝这次写入。
    [[nodiscard]] bool SaveToProto(CurrencyComp &proto) const
    {
        proto.clear_debts();
        for (const auto &[typeId, debt] : debts)
        {
            auto *entry = proto.add_debts();
            entry->set_currency_type(typeId);
            entry->set_owed(debt.owed);
            entry->set_paid(debt.paid);
            entry->set_frozen(debt.frozen);
            entry->set_expires_at(debt.expiresAt);
            entry->set_reason(debt.reason);
            entry->set_gm_operator(debt.gmOperator);
            entry->set_created_at(debt.createdAt);
            if (!unknown_field_carry::Restore(debt.unknownFields, *entry))
            {
                return false;
            }
        }
        return true;
    }
};
