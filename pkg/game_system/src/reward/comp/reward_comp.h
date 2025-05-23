﻿#pragma once

#include <bitset>

#include "cpp_table_id_bit_index/reward_table_id_bit_index.h"

// 使用 bitset 来表示奖励的领取状态
using RewardBitset = std::bitset<kRewardMaxBitIndex>;

struct RewardComp {
    RewardBitset rewards;
};