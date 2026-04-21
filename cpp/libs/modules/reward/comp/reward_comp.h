#pragma once

#include <bitset>

#include "table/code/bit_index/reward_table_id_bit_index.h"

// Bitset representing reward claim status.
// NOTE: Each business system (quest, achievement, daily-login, etc.) should own
// its own bitset with bit indices from that system's table, NOT from the reward table.
// The reward table ID is a pure data reference ("what items/currency to give"),
// not a persistence key for claimed status.
using RewardBitset = std::bitset<kRewardMaxBitIndex>;

// RewardComp: lightweight POD-level claimed-status component (ECS friendly).
// Use this only for global/one-time rewards not owned by a specific system
// (e.g., compensation, promotions). For quest/achievement rewards, each system
// should manage claimed state in its own component.
struct RewardComp {
	RewardBitset claimedRewards;
};