#pragma once

#include <unordered_map>

#include "modules/currency/constants/currency.h"
#include "proto/common/component/currency_comp.pb.h"

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
            debts[entry.currency_type()] = std::move(d);
        }
    }

    // Save runtime debts back into CurrencyComp.debts for persistence.
    void SaveToProto(CurrencyComp &proto) const
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
        }
    }
};
