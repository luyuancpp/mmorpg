#include "currency_system.h"

#include <algorithm>

#include <muduo/base/Logging.h>

#include "engine/core/error_handling/error_handling.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "core/utils/registry/game_registry.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "proto/common/component/currency_comp.pb.h"
#include <registry_manager.h>

// ---------------------------------------------------------------------------
// Internal helper
// ---------------------------------------------------------------------------

void CurrencySystem::EnsureCurrencySlots(CurrencyComp& currency)
{
    auto* values = currency.mutable_values();
    while (values->size() < static_cast<int>(kCurrencyMax))
    {
        values->Add(0);
    }
}

uint64_t* CurrencySystem::ResolveCurrencyField(entt::entity player, CurrencyType type)
{
    if (static_cast<uint32_t>(type) >= static_cast<uint32_t>(kCurrencyMax))
    {
        LOG_ERROR << "CurrencySystem: unknown CurrencyType=" << static_cast<uint32_t>(type)
                  << entt::to_integral(player);
        return nullptr;
    }

    auto* currency = tlsRegistryManager.actorRegistry.try_get<CurrencyComp>(player);
    if (currency == nullptr)
    {
        LOG_ERROR << "CurrencySystem: CurrencyComp missing on entity "
                  << entt::to_integral(player);
        return nullptr;
    }

    EnsureCurrencySlots(*currency);
    auto* values = currency->mutable_values();
    return &(*values)[static_cast<int>(type)];
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

    // ── GM block check ───────────────────────────────────────────────────
    if (IsCurrencyBlocked(player, type))
    {
        LOG_WARN << "CurrencySystem::AddCurrency: currency blocked by GM. CurrencyType="
                 << static_cast<uint32_t>(type) << " entity=" << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    uint64_t gain = static_cast<uint64_t>(amount);

    uint64_t* balance = ResolveCurrencyField(player, type);
    if (balance == nullptr)
    {
        return PrintStackAndReturnError(kInvalidParameter);
    }

    // ── Deferred-clawback / 补缴 deduction ──────────────────────────────
    auto* comp = tlsRegistryManager.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        comp = &tlsRegistryManager.actorRegistry.get_or_emplace<PlayerCurrencyComp>(player);
    }
    auto it = comp->debts.find(static_cast<uint32_t>(type));
    if (it != comp->debts.end())
    {
        auto& debt = it->second;
        uint64_t remaining = debt.Remaining();
        if (remaining > 0)
        {
            uint64_t deduct = std::min(gain, remaining);
            debt.paid += deduct;
            gain -= deduct;

            LOG_INFO << "CurrencySystem: deferred clawback deducted " << deduct
                     << " from gain, debt remaining=" << debt.Remaining()
                     << " CurrencyType=" << static_cast<uint32_t>(type)
                     << " entity=" << entt::to_integral(player);

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

    uint64_t* balance = ResolveCurrencyField(player, type);
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

    *balance -= cost;

    auto* comp = tlsRegistryManager.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        comp = &tlsRegistryManager.actorRegistry.get_or_emplace<PlayerCurrencyComp>(player);
    }
    comp->dirty = true;
    return kSuccess;
}

// ---------------------------------------------------------------------------
// GetBalance
// ---------------------------------------------------------------------------

uint64_t CurrencySystem::GetBalance(entt::entity player, CurrencyType type)
{
    const uint64_t* balance = ResolveCurrencyField(player, type);
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

void CurrencySystem::AttachDebt(entt::entity player, CurrencyType type, int64_t oweAmount)
{
    if (oweAmount <= 0)
    {
        LOG_ERROR << "CurrencySystem::AttachDebt: oweAmount must be > 0, got "
                  << oweAmount << " CurrencyType=" << static_cast<uint32_t>(type)
                  << " entity=" << entt::to_integral(player);
        return;
    }

    uint64_t debtToAdd = static_cast<uint64_t>(oweAmount);

    auto* comp = tlsRegistryManager.actorRegistry.try_get<PlayerCurrencyComp>(player);
    if (comp == nullptr)
    {
        LOG_ERROR << "CurrencySystem::AttachDebt: PlayerCurrencyComp missing on entity "
                  << entt::to_integral(player);
        return;
    }

    auto& debt = comp->debts[static_cast<uint32_t>(type)];
    debt.owed += debtToAdd;

    LOG_INFO << "CurrencySystem: debt attached, total owed=" << debt.owed
             << " paid=" << debt.paid
             << " CurrencyType=" << static_cast<uint32_t>(type)
             << " entity=" << entt::to_integral(player);
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

    auto* currency = tlsRegistryManager.actorRegistry.try_get<CurrencyComp>(player);
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

    auto* currency = tlsRegistryManager.actorRegistry.try_get<CurrencyComp>(player);
    if (currency == nullptr)
    {
        LOG_ERROR << "CurrencySystem::UnblockCurrency: CurrencyComp missing on entity "
                  << entt::to_integral(player);
        return PrintStackAndReturnError(kInvalidParameter);
    }

    const uint32_t typeId = static_cast<uint32_t>(type);
    auto* blocked = currency->mutable_blocked_types();
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
    const auto* currency = tlsRegistryManager.actorRegistry.try_get<CurrencyComp>(player);
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
