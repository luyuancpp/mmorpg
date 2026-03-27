#include "currency_system.h"

#include <algorithm>

#include <muduo/base/Logging.h>

#include "engine/core/error_handling/error_handling.h"
#include "engine/core/time/system/time.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "core/utils/registry/game_registry.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/gain_block/gain_block_service.h"
#include "modules/transaction_log/anomaly_detector.h"
#include "modules/transaction_log/transaction_log_system.h"
#include "proto/common/component/currency_comp.pb.h"
#include <registry_manager.h>

// ---------------------------------------------------------------------------
// Internal helper
// ---------------------------------------------------------------------------

void CurrencySystem::EnsureCurrencySlots(CurrencyComp &currency)
{
    auto *values = currency.mutable_values();
    while (values->size() < static_cast<int>(kCurrencyMax))
    {
        values->Add(0);
    }
}

uint64_t *CurrencySystem::ResolveCurrencyField(entt::entity player, CurrencyType type)
{
    if (static_cast<uint32_t>(type) >= static_cast<uint32_t>(kCurrencyMax))
    {
        LOG_ERROR << "CurrencySystem: unknown CurrencyType=" << static_cast<uint32_t>(type)
                  << entt::to_integral(player);
        return nullptr;
    }

    auto *currency = tlsEcs.actorRegistry.try_get<CurrencyComp>(player);
    if (currency == nullptr)
    {
        LOG_ERROR << "CurrencySystem: CurrencyComp missing on entity "
                  << entt::to_integral(player);
        return nullptr;
    }

    EnsureCurrencySlots(*currency);
    auto *values = currency->mutable_values();
    return &(*values)[static_cast<uint32_t>(type)];
}

// ---------------------------------------------------------------------------
// AddCurrency — the single unified entry point for all currency gains.
// ---------------------------------------------------------------------------

uint32_t CurrencySystem::AddCurrency(entt::entity player, CurrencyType type, int64_t amount)
{
    // ── Strict parameter validation ──────────────────────────────────────
    if (amount <= 0)
    {
        LOG_ERROR << "CurrencySystem::AddCurrency: amount must be > 0, got "
                  << amount << " for CurrencyType=" << static_cast<uint32_t>(type)
                  << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    // ── Global (server-wide) block check ────────────────────────────────
    if (GainBlockService::IsGainBlocked(GainBlockService::GainType::kCurrency,
                                        static_cast<uint32_t>(type)))
    {
        LOG_WARN << "CurrencySystem::AddCurrency: currency GLOBALLY blocked. CurrencyType="
                 << static_cast<uint32_t>(type) << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    // ── Per-player GM block check ────────────────────────────────────────
    if (IsCurrencyBlocked(player, type))
    {
        LOG_WARN << "CurrencySystem::AddCurrency: currency blocked by GM. CurrencyType="
                 << static_cast<uint32_t>(type) << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    uint64_t gain = static_cast<uint64_t>(amount);

    uint64_t *balance = ResolveCurrencyField(player, type);
    if (balance == nullptr)
    {
        return PrintStackAndReturnError(kInvalidParameter);
    }

    const uint64_t balanceBefore = *balance;

    // ── Deferred-clawback / 补缴 deduction ──────────────────────────────
    auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        comp = &tlsEcs.actorRegistry.get_or_emplace<PlayerCurrencyComp>(player);
    }
    auto it = comp->debts.find(static_cast<uint32_t>(type));
    if (it != comp->debts.end())
    {
        auto &debt = it->second;
        uint64_t remaining = debt.Remaining();
        // Only deduct if the debt is active (not frozen and not expired).
        const bool frozen = debt.frozen;
        const bool expired = (debt.expiresAt > 0 && TimeSystem::NowSecondsUTC() >= debt.expiresAt);
        if (remaining > 0 && !frozen && !expired)
        {
            uint64_t deduct = std::min(gain, remaining);
            debt.paid += deduct;
            gain -= deduct;

            LOG_INFO << "CurrencySystem: deferred clawback deducted " << deduct
                     << " from gain, debt remaining=" << debt.Remaining()
                     << " CurrencyType=" << static_cast<uint32_t>(type)
                     << " entity=" << entt::to_integral(player);

            // Emit clawback tx log entry.
            TransactionLogSystem::LogClawbackDeduction(player, type, deduct, debt.Remaining());

            if (debt.Remaining() <= 0)
            {
                comp->debts.erase(it);
            }
        }
    }

    // ── Credit remaining amount after debt ───────────────────────────────
    if (gain > 0)
    {
        *balance += gain;
    }

    comp->dirty = true;

    // ── Transaction log ──────────────────────────────────────────────────
    TransactionLogSystem::LogCurrencyAdd(player, type,
                                         static_cast<uint64_t>(amount), balanceBefore, *balance);

    // ── Anomaly detection ────────────────────────────────────────────────
    AnomalyDetector::RecordCurrencyGain(player, type, static_cast<uint64_t>(amount));

    return kSuccess;
}

// ---------------------------------------------------------------------------
// DeductCurrency
// ---------------------------------------------------------------------------

uint32_t CurrencySystem::DeductCurrency(entt::entity player, CurrencyType type, int64_t amount)
{
    // ── Strict parameter validation ──────────────────────────────────────
    if (amount <= 0)
    {
        LOG_ERROR << "CurrencySystem::DeductCurrency: amount must be > 0, got "
                  << amount << " for CurrencyType=" << static_cast<uint32_t>(type)
                  << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    uint64_t cost = static_cast<uint64_t>(amount);

    uint64_t *balance = ResolveCurrencyField(player, type);
    if (balance == nullptr)
    {
        return PrintStackAndReturnError(kInvalidParameter);
    }

    if (*balance < cost)
    {
        LOG_WARN << "CurrencySystem::DeductCurrency: insufficient funds. balance="
                 << *balance << " requested=" << cost
                 << " CurrencyType=" << static_cast<uint32_t>(type)
                 << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    const uint64_t balanceBefore = *balance;
    *balance -= cost;

    auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        comp = &tlsEcs.actorRegistry.get_or_emplace<PlayerCurrencyComp>(player);
    }
    comp->dirty = true;

    // ── Transaction log ──────────────────────────────────────────────────
    TransactionLogSystem::LogCurrencyDeduct(player, type, cost, balanceBefore, *balance);

    return kSuccess;
}

// ---------------------------------------------------------------------------
// GetBalance
// ---------------------------------------------------------------------------

uint64_t CurrencySystem::GetBalance(entt::entity player, CurrencyType type)
{
    const uint64_t *balance = ResolveCurrencyField(player, type);
    return balance ? *balance : 0;
}

// ---------------------------------------------------------------------------
// CanAfford
// ---------------------------------------------------------------------------

bool CurrencySystem::CanAfford(entt::entity player, CurrencyType type, int64_t amount)
{
    if (amount <= 0)
    {
        LOG_ERROR << "CurrencySystem::CanAfford: amount must be > 0, got "
                  << amount << " CurrencyType=" << static_cast<uint32_t>(type)
                  << " entity=" << entt::to_integral(player);
        return false;
    }
    return GetBalance(player, type) >= static_cast<uint64_t>(amount);
}

// ---------------------------------------------------------------------------
// AttachDebt (补缴)
// ---------------------------------------------------------------------------

void CurrencySystem::AttachDebt(entt::entity player, CurrencyType type, int64_t oweAmount,
                                const std::string &reason,
                                const std::string &gmOperator,
                                uint64_t expiresAt)
{
    if (oweAmount <= 0)
    {
        LOG_ERROR << "CurrencySystem::AttachDebt: oweAmount must be > 0, got "
                  << oweAmount << " CurrencyType=" << static_cast<uint32_t>(type)
                  << " entity=" << entt::to_integral(player);
        return;
    }

    uint64_t debtToAdd = static_cast<uint64_t>(oweAmount);

    auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        LOG_ERROR << "CurrencySystem::AttachDebt: PlayerCurrencyComp missing on entity "
                  << entt::to_integral(player);
        return;
    }

    auto &debt = comp->debts[static_cast<uint32_t>(type)];
    debt.owed += debtToAdd;
    debt.reason = reason;
    debt.gmOperator = gmOperator;
    debt.expiresAt = expiresAt;
    if (debt.createdAt == 0)
    {
        debt.createdAt = TimeSystem::NowSecondsUTC();
    }

    comp->dirty = true;

    LOG_INFO << "CurrencySystem: debt attached, total owed=" << debt.owed
             << " paid=" << debt.paid
             << " CurrencyType=" << static_cast<uint32_t>(type)
             << " entity=" << entt::to_integral(player);
}

// ---------------------------------------------------------------------------
// WaiveDebt — write off remaining debt
// ---------------------------------------------------------------------------

uint64_t CurrencySystem::WaiveDebt(entt::entity player, CurrencyType type,
                                   const std::string &gmOperator,
                                   const std::string &reason)
{
    auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        return 0;
    }

    auto it = comp->debts.find(static_cast<uint32_t>(type));
    if (it == comp->debts.end())
    {
        return 0;
    }

    const uint64_t waived = it->second.Remaining();
    comp->debts.erase(it);
    comp->dirty = true;

    LOG_INFO << "CurrencySystem: debt waived, amount=" << waived
             << " CurrencyType=" << static_cast<uint32_t>(type)
             << " operator=" << gmOperator
             << " reason=" << reason
             << " entity=" << entt::to_integral(player);
    return waived;
}

// ---------------------------------------------------------------------------
// AdjustDebt — increase or decrease the owed amount
// ---------------------------------------------------------------------------

void CurrencySystem::AdjustDebt(entt::entity player, CurrencyType type, int64_t delta,
                                const std::string &gmOperator,
                                const std::string &reason)
{
    auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        LOG_ERROR << "CurrencySystem::AdjustDebt: PlayerCurrencyComp missing on entity "
                  << entt::to_integral(player);
        return;
    }

    auto it = comp->debts.find(static_cast<uint32_t>(type));
    if (it == comp->debts.end())
    {
        if (delta <= 0)
        {
            return; // Nothing to decrease
        }
        // Create new debt
        CurrencyDebt newDebt;
        newDebt.owed = static_cast<uint64_t>(delta);
        newDebt.createdAt = TimeSystem::NowSecondsUTC();
        newDebt.reason = reason;
        newDebt.gmOperator = gmOperator;
        comp->debts[static_cast<uint32_t>(type)] = std::move(newDebt);
        comp->dirty = true;
        return;
    }

    auto &debt = it->second;
    if (delta > 0)
    {
        debt.owed += static_cast<uint64_t>(delta);
    }
    else
    {
        uint64_t decrease = static_cast<uint64_t>(-delta);
        // Clamp so owed never goes below paid
        if (debt.owed - decrease < debt.paid)
        {
            debt.owed = debt.paid; // effectively zeroes remaining
        }
        else
        {
            debt.owed -= decrease;
        }
    }

    if (!gmOperator.empty())
    {
        debt.gmOperator = gmOperator;
    }
    if (!reason.empty())
    {
        debt.reason = reason;
    }

    comp->dirty = true;

    LOG_INFO << "CurrencySystem: debt adjusted, owed=" << debt.owed
             << " paid=" << debt.paid
             << " delta=" << delta
             << " CurrencyType=" << static_cast<uint32_t>(type)
             << " entity=" << entt::to_integral(player);
}

// ---------------------------------------------------------------------------
// FreezeDebt — pause/resume auto-deduction
// ---------------------------------------------------------------------------

void CurrencySystem::FreezeDebt(entt::entity player, CurrencyType type, bool freeze,
                                const std::string &gmOperator)
{
    auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        return;
    }

    auto it = comp->debts.find(static_cast<uint32_t>(type));
    if (it == comp->debts.end())
    {
        return;
    }

    it->second.frozen = freeze;
    if (!gmOperator.empty())
    {
        it->second.gmOperator = gmOperator;
    }
    comp->dirty = true;

    LOG_INFO << "CurrencySystem: debt " << (freeze ? "frozen" : "unfrozen")
             << " CurrencyType=" << static_cast<uint32_t>(type)
             << " operator=" << gmOperator
             << " entity=" << entt::to_integral(player);
}

// ---------------------------------------------------------------------------
// QueryDebts — return all active debts for GM inspection
// ---------------------------------------------------------------------------

std::vector<CurrencyDebtEntry> CurrencySystem::QueryDebts(entt::entity player)
{
    std::vector<CurrencyDebtEntry> result;
    const auto *comp = tlsEcs.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        return result;
    }

    result.reserve(comp->debts.size());
    for (const auto &[typeId, debt] : comp->debts)
    {
        CurrencyDebtEntry entry;
        entry.set_currency_type(typeId);
        entry.set_owed(debt.owed);
        entry.set_paid(debt.paid);
        entry.set_frozen(debt.frozen);
        entry.set_expires_at(debt.expiresAt);
        entry.set_reason(debt.reason);
        entry.set_gm_operator(debt.gmOperator);
        entry.set_created_at(debt.createdAt);
        result.push_back(std::move(entry));
    }
    return result;
}

// ---------------------------------------------------------------------------
// BlockCurrency — GM禁止获取某种货币
// ---------------------------------------------------------------------------

uint32_t CurrencySystem::BlockCurrency(entt::entity player, CurrencyType type)
{
    if (static_cast<uint32_t>(type) >= static_cast<uint32_t>(kCurrencyMax))
    {
        LOG_ERROR << "CurrencySystem::BlockCurrency: invalid CurrencyType="
                  << static_cast<uint32_t>(type) << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    auto *currency = tlsEcs.actorRegistry.try_get<CurrencyComp>(player);
    if (currency == nullptr)
    {
        LOG_ERROR << "CurrencySystem::BlockCurrency: CurrencyComp missing on entity "
                  << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    const uint32_t typeId = static_cast<uint32_t>(type);
    // Avoid duplicate entries.
    for (int i = 0; i < currency->blocked_types_size(); ++i)
    {
        if (currency->blocked_types(i) == typeId)
        {
            return kSuccess; // already blocked
        }
    }
    currency->add_blocked_types(typeId);

    LOG_INFO << "CurrencySystem: GM blocked CurrencyType=" << typeId
             << " entity=" << entt::to_integral(player);
    return kSuccess;
}

// ---------------------------------------------------------------------------
// UnblockCurrency — GM解除禁止获取
// ---------------------------------------------------------------------------

uint32_t CurrencySystem::UnblockCurrency(entt::entity player, CurrencyType type)
{
    if (static_cast<uint32_t>(type) >= static_cast<uint32_t>(kCurrencyMax))
    {
        LOG_ERROR << "CurrencySystem::UnblockCurrency: invalid CurrencyType="
                  << static_cast<uint32_t>(type) << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    auto *currency = tlsEcs.actorRegistry.try_get<CurrencyComp>(player);
    if (currency == nullptr)
    {
        LOG_ERROR << "CurrencySystem::UnblockCurrency: CurrencyComp missing on entity "
                  << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    const uint32_t typeId = static_cast<uint32_t>(type);
    auto *blocked = currency->mutable_blocked_types();
    for (int i = 0; i < blocked->size(); ++i)
    {
        if ((*blocked)[i] == typeId)
        {
            blocked->SwapElements(i, blocked->size() - 1);
            blocked->RemoveLast();
            LOG_INFO << "CurrencySystem: GM unblocked CurrencyType=" << typeId
                     << " entity=" << entt::to_integral(player);
            return kSuccess;
        }
    }
    return kSuccess; // was not blocked
}

// ---------------------------------------------------------------------------
// IsCurrencyBlocked
// ---------------------------------------------------------------------------

bool CurrencySystem::IsCurrencyBlocked(entt::entity player, CurrencyType type)
{
    const auto *currency = tlsEcs.actorRegistry.try_get<CurrencyComp>(player);
    if (currency == nullptr)
    {
        return false;
    }
    const uint32_t typeId = static_cast<uint32_t>(type);
    for (int i = 0; i < currency->blocked_types_size(); ++i)
    {
        if (currency->blocked_types(i) == typeId)
        {
            return true;
        }
    }
    return false;
}
