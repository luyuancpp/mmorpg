#pragma once

#include <bitset>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "table/code/bit_index/reward_table_id_bit_index.h"
#include <modules/reward/constants/reward.h>

// 使用 bitset 来表示奖励的领取状态（用于模板索引）
using RewardBitset = std::bitset<kRewardMaxBitIndex>;

struct RewardComp {
public:
	RewardComp() {
		// 预留一点桶，减少运行时频繁 rehash（根据实际 instance 数调整）
		instance_rewards.reserve(4);
		// 保证存在全局/永久 entry，统一模型（0 表示全局）
		instance_rewards.try_emplace(kGlobalInstanceId, RewardBitset{});
	}

	// 非 const 全局访问
	RewardBitset& GlobalRewards() {
		return GetOrCreateInstance(kGlobalInstanceId);
	}

	// 新增：const 全局访问（安全返回，若不存在返回静态空实例）
	const RewardBitset& GlobalRewards() const {
		auto it = instance_rewards.find(kGlobalInstanceId);
		if (it == instance_rewards.end()) {
			static const RewardBitset kEmpty{};
			return kEmpty;
		}
		return it->second;
	}

	const RewardBitset* GetGlobalInstance() const {
		return GetInstance(kGlobalInstanceId);
	}

	// 获取或创建对应 instance 的 bitset（返回引用，注意可能被 later insert 导致失效）
	// 推荐尽量使用下面的封装 API（SetClaimed/IsClaimed 等），不要长期保存引用
	RewardBitset& GetOrCreateInstance(InstanceId instanceId) {
		auto [it, inserted] = instance_rewards.try_emplace(instanceId, RewardBitset{});
		return it->second;
	}

	// 只读获取（可能为 nullptr）
	const RewardBitset* GetInstance(InstanceId instanceId) const {
		auto it = instance_rewards.find(instanceId);
		return (it == instance_rewards.end()) ? nullptr : &it->second;
	}

	// 删除并返回是否存在（用于归档/重置）
	bool EraseInstance(InstanceId instanceId) {
		return instance_rewards.erase(instanceId) > 0;
	}

	// 封装操作：设置某 templateIndex 为已领取（幂等）
	// 返回： true = 成功（之前未领且已标记）， false = 已领取或参数无效
	bool SetClaimed(InstanceId instanceId, uint32_t templateIndex) {
		if (templateIndex >= kRewardMaxBitIndex) return false;
		auto& bits = GetOrCreateInstance(instanceId);
		if (bits.test(templateIndex)) return false;
		bits.set(templateIndex);
		return true;
	}

	// 检查是否已领取（优雅的查找：先 instance，再回退到全局可选）
	bool IsClaimed(InstanceId instanceId, uint32_t templateIndex, bool fallbackToGlobal = true) const {
		if (templateIndex >= kRewardMaxBitIndex) return false;
		if (auto p = GetInstance(instanceId); p != nullptr) {
			if (p->test(templateIndex)) return true;
		}
		if (fallbackToGlobal && instanceId != kGlobalInstanceId) {
			if (auto g = GetInstance(kGlobalInstanceId); g != nullptr) {
				return g->test(templateIndex);
			}
		}
		return false;
	}

	// 清除单个 claim（用于撤销或重置），返回是否有变更
	bool ClearClaim(InstanceId instanceId, uint32_t templateIndex) {
		if (templateIndex >= kRewardMaxBitIndex) return false;
		auto it = instance_rewards.find(instanceId);
		if (it == instance_rewards.end()) return false;
		if (!it->second.test(templateIndex)) return false;
		it->second.reset(templateIndex);
		return true;
	}

	// 清除整个 instance（归档后调用）
	void ClearInstance(InstanceId instanceId) {
		instance_rewards.erase(instanceId);
	}

	// 简单枚举接口（只列举已存在的 instance id）
	// 注意：返回 vector 会复制 keys，避免外部迭代期间对容器修改
	std::vector<InstanceId> ListInstanceIds() const {
		std::vector<InstanceId> keys;
		keys.reserve(instance_rewards.size());
		for (const auto& p : instance_rewards) keys.push_back(p.first);
		return keys;
	}

private:
	// key: instance id (uint64_t), value: 对应该 instance 的 RewardBitset
	// 约定：key == kGlobalInstanceId 表示永久/一次性模板集合
	std::unordered_map<InstanceId, RewardBitset> instance_rewards;
};