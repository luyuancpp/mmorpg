#pragma once

#include <string>
#include <vector>

#include "entt/src/entt/entity/entity.hpp"
#include "modules/currency/constants/currency.h"
#include "proto/common/component/currency_comp.pb.h"

// Stateless system that handles all currency mutations.
// Every add/deduct goes through this system — no direct field writes.
class CurrencySystem
{
public:
    // Add currency to a player. amount must be > 0; returns error code otherwise.
    // Automatically deducts from any outstanding debt (补缴) before crediting.
    // Writes a TransactionLogEntry to Kafka.
    static uint32_t AddCurrency(entt::entity player, CurrencyType type, int64_t amount);

    // Deduct currency from a player. amount must be > 0; returns error code if
    // the parameter is invalid or the player has insufficient funds.
    // Writes a TransactionLogEntry to Kafka.
    static uint32_t DeductCurrency(entt::entity player, CurrencyType type, int64_t amount);

    // Query current balance (returns 0 if component missing).
    static uint64_t GetBalance(entt::entity player, CurrencyType type);

    // Check whether the player can afford `amount` of `type`.
    static bool CanAfford(entt::entity player, CurrencyType type, int64_t amount);

    // ── Deferred Clawback (补缴) — GM debt management ───────────────────

    // Attach a deferred-clawback debt.  Future AddCurrency calls will
    // automatically deduct from gains until the debt is repaid.
    static void AttachDebt(entt::entity player, CurrencyType type, int64_t oweAmount,
                           const std::string &reason = {},
                           const std::string &gmOperator = {},
                           uint64_t expiresAt = 0);

    // Write off all remaining debt for the given currency type.
    static uint64_t WaiveDebt(entt::entity player, CurrencyType type,
                              const std::string &gmOperator = {},
                              const std::string &reason = {});

    // Adjust the owed amount by `delta` (positive = increase, negative = decrease).
    // Clamps so that owed never goes below paid.
    static void AdjustDebt(entt::entity player, CurrencyType type, int64_t delta,
                           const std::string &gmOperator = {},
                           const std::string &reason = {});

    // Freeze or unfreeze auto-deduction for a currency type's debt.
    static void FreezeDebt(entt::entity player, CurrencyType type, bool freeze,
                           const std::string &gmOperator = {});

    // Return all active debts for the player (for GM query).
    static std::vector<CurrencyDebtEntry> QueryDebts(entt::entity player);

    // ── GM block/unblock ─────────────────────────────────────────────────

    // GM: block a currency type — all future AddCurrency calls for this type will be rejected.
    static uint32_t BlockCurrency(entt::entity player, CurrencyType type);

    // GM: unblock a previously blocked currency type.
    static uint32_t UnblockCurrency(entt::entity player, CurrencyType type);

    // Query whether a currency type is currently blocked.
    static bool IsCurrencyBlocked(entt::entity player, CurrencyType type);

private:
    // Ensure proto slot array has at least kCurrencyMax elements.
    static void EnsureCurrencySlots(CurrencyComp &currency);

    // Resolve the uint64 slot pointer from proto CurrencyComp.
    // Returns nullptr + logs error when type is out of range.
    static uint64_t *ResolveCurrencyField(entt::entity player, CurrencyType type);
};
