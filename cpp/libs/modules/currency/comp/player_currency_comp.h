#pragma once

#include <unordered_map>

#include "modules/currency/constants/currency.h"

// Per-player deferred-clawback debt record.
struct CurrencyDebt
{
    uint64_t owed{0};
    uint64_t paid{0};

    [[nodiscard]] uint64_t Remaining() const { return owed > paid ? (owed - paid) : 0; }
};

// Runtime-only metadata. Persistent balances are stored in proto CurrencyComp.
struct PlayerCurrencyComp
{
    // Deferred-clawback debts (补缴), keyed by CurrencyType.
    std::unordered_map<uint32_t, CurrencyDebt> debts;

    bool dirty{false};
};
