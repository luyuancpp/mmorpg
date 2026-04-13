#pragma once

#include <cstdint>
#include <unordered_set>

// GainBlockService: unified block/unblock for ALL gain channels.
//
// Two levels of blocking:
//   1. Per-player block — stored in per-player proto components
//      (CurrencyComp.blocked_types  for currency,
//       PlayerItemBlockComp.blocked_item_config_ids for items).
//      These are managed by CurrencySystem and Bag directly.
//
//   2. Server-wide (global) block — thread_local sets, applied to ALL
//      players on this thread.  Intended for emergency hotfix scenarios
//      (e.g. "block everyone from gaining item 30045 until the dupe bug
//      is fixed").  Loaded from GM config / hot-reload.
//
// Entry-point check order (in AddCurrency / AddItem):
//   1. Global block → reject immediately
//   2. Per-player block → reject immediately
//   3. Normal gain logic
//
// Thread safety: all storage is thread_local (single-threaded ECS per
// scene thread), same pattern as AnomalyDetector.

class GainBlockService
{
public:
    enum class GainType : uint32_t
    {
        kCurrency = 0,
        kItem     = 1,
    };

    // ── Server-wide (global) blocks ──────────────────────────────────────

    // Block a specific ID globally for all players on this thread.
    static void BlockGlobal(GainType type, uint32_t id);

    // Unblock a previously globally blocked ID.
    static void UnblockGlobal(GainType type, uint32_t id);

    // Check whether an ID is globally blocked.
    static bool IsGloballyBlocked(GainType type, uint32_t id);

    // Return all globally blocked IDs of a given type (for GM query).
    static const std::unordered_set<uint32_t>& GlobalBlockedIds(GainType type);

    // Clear all global blocks (for server restart or GM reset).
    static void ClearAllGlobalBlocks();

    // ── Convenience: combined check (global + per-player) ────────────────

    // Returns true if the ID is blocked either globally OR per-player.
    // For currency: checks global + CurrencyComp.blocked_types.
    // For items:    checks global + Bag::IsItemBlocked.
    // Callers can use this as the single "should I block?" entry point.
    static bool IsGainBlocked(GainType type, uint32_t id);
};
