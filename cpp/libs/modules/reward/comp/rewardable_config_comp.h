#pragma once

#include <cstdint>

// Minimal interface for any system whose entries can carry a reward.
// Implement this in MissionConfig, AchievementConfig, SignInConfig, etc.
// so that reward-dispatch logic can be written once against this interface.
struct IRewardableConfig
{
    virtual ~IRewardableConfig() = default;

    // Whether the table contains an entry for |id|.
    virtual bool HasKey(uint32_t id) const = 0;

    // Return the reward-table ID associated with |id|, or 0 if none.
    virtual uint32_t GetRewardId(uint32_t id) const = 0;

    // Whether the reward for |id| should be granted automatically on completion.
    virtual bool AutoReward(uint32_t id) const = 0;
};
