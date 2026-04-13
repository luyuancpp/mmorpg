#pragma once

#include <cstdint>
#include <string>
#include "entt/src/entt/entity/entity.hpp"
#include "modules/currency/constants/currency.h"
#include "proto/common/rollback/transaction_log.pb.h"

// Kafka topic for transaction log entries.
constexpr char kTransactionLogTopic[] = "transaction_log_topic";

// Stateless utility that builds TransactionLogEntry messages and sends them
// to Kafka for persistence by the Go DB service.
//
// Usage:
//   TransactionLogSystem::LogCurrencyAdd(player, kCurrencyGold, 100, balanceBefore, balanceAfter);
//   TransactionLogSystem::LogItemTransfer(fromPlayer, toPlayer, itemUuid, configId, qty, TX_TRADE, corrId);
//
// All methods are fire-and-forget: failures are logged but never block gameplay.
class TransactionLogSystem
{
public:
    // ── Currency operations ──────────────────────────────────────────────

    static void LogCurrencyAdd(
        entt::entity player,
        CurrencyType type,
        uint64_t amount,
        uint64_t balanceBefore,
        uint64_t balanceAfter,
        TransactionType txType = TX_CURRENCY_ADD);

    static void LogCurrencyDeduct(
        entt::entity player,
        CurrencyType type,
        uint64_t amount,
        uint64_t balanceBefore,
        uint64_t balanceAfter,
        TransactionType txType = TX_CURRENCY_DEDUCT);

    // Deferred clawback deduction event.
    static void LogClawbackDeduction(
        entt::entity player,
        CurrencyType type,
        uint64_t deductedAmount,
        uint64_t debtRemaining);

    // ── Item operations ──────────────────────────────────────────────────

    // Log an item being transferred between two players (or player ↔ system).
    static void LogItemTransfer(
        uint64_t fromPlayerId,
        uint64_t toPlayerId,
        uint64_t itemUuid,
        uint32_t configId,
        uint32_t quantity,
        TransactionType txType,
        uint64_t correlationId = 0);

    // Log an item being created (quest reward, GM grant, etc.).
    static void LogItemCreate(
        entt::entity player,
        uint64_t itemUuid,
        uint32_t configId,
        uint32_t quantity,
        TransactionType txType);

    // Log an item being destroyed / consumed.
    static void LogItemDestroy(
        entt::entity player,
        uint64_t itemUuid,
        uint32_t configId,
        uint32_t quantity);

    // ── Generic / low-level ──────────────────────────────────────────────

    // Build and send a fully custom entry.
    static void SendEntry(const TransactionLogEntry &entry);

private:
    // Get the player_id from an entity. Returns 0 if not resolvable.
    static uint64_t ResolvePlayerId(entt::entity player);

    // Generate a unique tx_id via SnowFlake.
    static uint64_t GenerateTxId();
};
