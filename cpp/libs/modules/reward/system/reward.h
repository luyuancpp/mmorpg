#pragma once

#include <bitset>
#include <cstdint>

#include "modules/reward/comp/reward_comp.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"

// RewardClaimSystem: pure "dispatch reward items" utility.
// Claimed state is NOT managed here — each business system (quest, achievement, etc.)
// manages its own bitset. The template methods below are generic helpers that work
// with any std::bitset.
class RewardClaimSystem {
public:
    // Generic claim/check helpers — work with any std::bitset<N>
    template <std::size_t N>
    static uint32_t ClaimByIndex(uint32_t index, std::bitset<N>& bits);

    template <std::size_t N>
    static bool IsClaimedByIndex(uint32_t index, const std::bitset<N>& bits);

    template <std::size_t N>
    static bool HasUnclaimed(const std::bitset<N>& bits);
};

// --- Template implementations ---

template <std::size_t N>
uint32_t RewardClaimSystem::ClaimByIndex(uint32_t index, std::bitset<N>& bits) {
    if (index >= N) {
        return kIndexOutOfRange;
    }
    if (bits.test(index)) {
        return kRewardAlreadyClaimed;
    }
    bits.set(index);
    return kSuccess;
}

template <std::size_t N>
bool RewardClaimSystem::IsClaimedByIndex(uint32_t index, const std::bitset<N>& bits) {
    if (index >= N) {
        return false;
    }
    return bits[index];
}

template <std::size_t N>
bool RewardClaimSystem::HasUnclaimed(const std::bitset<N>& bits) {
    return bits.count() < N;
}

