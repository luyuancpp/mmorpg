#pragma once

#include <bitset>
#include <cstdint>
#include <unordered_map>

#include "table/code/bit_index/reward_table_id_bit_index.h"

// 使用 bitset 来表示奖励的领取状态（用于模板索引）
using RewardBitset = std::bitset<kRewardMaxBitIndex>;

// 统一 instance id 类型（建议 uint64_t）
using InstanceId = uint64_t;
constexpr InstanceId kGlobalInstanceId = 0; // 约定：0 表示全局/永久模板（一次性）

struct RewardComp {
	RewardBitset rewards;

	// 按 instanceId 存储的领取记录（稀疏）
	// key: instance id (uint64_t), value: 对应该 instance 的 RewardBitset
	// 约定：key == kGlobalInstanceId 表示永久/一次性模板集合
	std::unordered_map<InstanceId, RewardBitset> instance_rewards;

	// 获取或创建对应 instance 的 bitset（包含全局）
	RewardBitset* GetOrCreateInstance(InstanceId instanceId = kGlobalInstanceId) {
		auto it = instance_rewards.find(instanceId);
		if (it == instance_rewards.end()) {
			auto [newIt, _] = instance_rewards.emplace(instanceId, RewardBitset{});
			return &newIt->second;
		}
		return &it->second;
	}

	// 只读获取（可能为 nullptr）
	const RewardBitset* GetInstance(InstanceId instanceId = kGlobalInstanceId) const {
		auto it = instance_rewards.find(instanceId);
		return (it == instance_rewards.end()) ? nullptr : &it->second;
	}

	// 删除并返回是否存在（delete 表示重置/归档）
	bool EraseInstance(InstanceId instanceId) {
		return instance_rewards.erase(instanceId) > 0;
	}
};