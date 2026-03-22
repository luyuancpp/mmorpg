#pragma once

#include <bitset>
#include <cstdint>
#include <optional>
#include <unordered_map>

// Bit-index map type shared by all generated *_table_id_bit_index.h headers.
using BitIndexMap = std::unordered_map<uint64_t, uint32_t>;

// Look up the bit position for |id| in |bitMap|.
// Returns std::nullopt when the id has no mapping (table was changed / id invalid).
inline std::optional<uint32_t> GetBitIndex(const BitIndexMap& bitMap, uint64_t id)
{
    if (const auto it = bitMap.find(id); it != bitMap.end())
    {
        return it->second;
    }
    return std::nullopt;
}

// Test whether |id| is marked in |bits| according to |bitMap|.
// Returns false when id is not present in bitMap.
template <std::size_t N>
bool TestBit(const BitIndexMap& bitMap, const std::bitset<N>& bits, uint64_t id)
{
    const auto idx = GetBitIndex(bitMap, id);
    return idx.has_value() && bits.test(*idx);
}

// Set / clear the bit for |id| in |bits| according to |bitMap|.
// Returns false (no-op) when id is not present in bitMap.
template <std::size_t N>
bool SetBit(const BitIndexMap& bitMap, std::bitset<N>& bits, uint64_t id, bool value = true)
{
    const auto idx = GetBitIndex(bitMap, id);
    if (!idx.has_value())
    {
        return false;
    }
    bits.set(*idx, value);
    return true;
}

// Reward-decision result for a completed entry.
enum class RewardAction : uint8_t
{
    kNone,       // no reward configured
    kAutoGrant,  // reward should be granted immediately
    kClaimable,  // reward exists but player must claim manually
};

// Determine what reward action to take for |id|.
// Config must provide: uint32_t GetRewardId(uint32_t) const;
//                      bool     AutoReward(uint32_t)  const;
// No inheritance required — any Config type with these two methods works.
template <typename Config>
RewardAction GetRewardAction(const Config& config, uint32_t id)
{
    if (config.GetRewardId(id) <= 0)
    {
        return RewardAction::kNone;
    }
    return config.AutoReward(id) ? RewardAction::kAutoGrant
                                 : RewardAction::kClaimable;
}
