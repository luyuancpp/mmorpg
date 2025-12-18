#include "reward.h"

#include "muduo/base/Logging.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include "reward/comp/reward.h"

#include <threading/registry_manager.h>
#include <sstream>

uint32_t RewardSystem::ClaimRewardByRewardId(uint32_t rewardId, RewardBitset& claimedRewards) {
	auto it = RewardBitMap.find(rewardId);
	if (it == RewardBitMap.end()) {
		return kInvalidTableId;
	}

	const auto rewardIndex = it->second;
	return ClaimRewardByIndex(rewardIndex, claimedRewards);
}

uint32_t RewardSystem::ClaimRewardByIndex(uint32_t rewardIndex, RewardBitset& claimedRewards) {
	if (rewardIndex >= kRewardMaxBitIndex) {
		return kIndexOutOfRange;
	}

	if (claimedRewards[rewardIndex]) {
		return kRewardAlreadyClaimed;
	}

	claimedRewards.set(rewardIndex);  // 设置该奖励为已领取
	return kSuccess;
}

bool RewardSystem::IsRewardClaimedById(uint32_t rewardId, const RewardBitset& claimedRewards)
{
	auto it = RewardBitMap.find(rewardId);
	if (it == RewardBitMap.end()) {
		return false;
	}

	const auto rewardIndex = it->second;
	return IsRewardClaimedByIndex(rewardIndex, claimedRewards);
}

bool RewardSystem::IsRewardClaimedByIndex(uint32_t rewardIndex, const RewardBitset& claimedRewards) {
	if (rewardIndex >= kRewardMaxBitIndex) {
		return false;
	}

	return claimedRewards[rewardIndex];
}

void RewardSystem::ShowRewardStatus() {
	LOG_INFO << "=== Displaying Reward Status for All Entities ===";

	// 获取所有拥有 RewardComp 的实体
	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[](entt::entity entity, const RewardComp& rewardComp) {
			ShowEntityRewardStatus(rewardComp.rewards);
		});

	LOG_INFO << "=== End of Reward Status Display ===";
}

// 显示单个实体的奖励状态
void RewardSystem::ShowEntityRewardStatus(const RewardBitset& claimedRewards) {
	// 遍历所有奖励
	for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
		const bool isClaimed = claimedRewards[i];
		LOG_INFO << "  Reward " << i << ": " << (isClaimed ? "Claimed" : "Not Claimed");
	}
}

void RewardSystem::CountRewardStatistics() {
	uint64_t totalClaimed = 0;
	uint64_t totalRewards = 0;

	tlsRegistryManager.actorRegistry.view<RewardComp>().each([&](entt::entity /*entity*/, const RewardComp& rewardComp) {
		// 如果 RewardBitset 是 std::bitset，可直接使用 count()
		totalClaimed += static_cast<uint64_t>(rewardComp.rewards.count());
		totalRewards += static_cast<uint64_t>(kRewardMaxBitIndex);
		});

	LOG_INFO << "Total Rewards: " << totalRewards
		<< ", Claimed: " << totalClaimed
		<< ", Unclaimed: " << (totalRewards - totalClaimed);
}

std::string RewardSystem::FormatRewardStatus(entt::entity entityId, const RewardBitset& claimedRewards) {
	std::ostringstream ss;
	ss << "Entity ID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(entityId) << "\n";

	for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
		ss << "  Reward " << i << ": " << (claimedRewards[i] ? "Claimed" : "Not Claimed") << "\n";
	}

	return ss.str();
}

bool RewardSystem::HasUnclaimedRewards(const RewardBitset& claimedRewards) {
	// 如果 RewardBitset 支持 count()
	if (claimedRewards.count() < kRewardMaxBitIndex) {
		return true;
	}
	return false;
}

// 例如仅显示未领取奖励的实体
void RewardSystem::ShowUnclaimedRewards() {
	LOG_INFO << "=== Displaying Entities with Unclaimed Rewards ===";

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[](entt::entity entity, const RewardComp& rewardComp) {
			if (HasUnclaimedRewards(rewardComp.rewards)) {
				LOG_INFO << "Entity ID with unclaimed rewards: " << entt::to_integral(entity);
			}
		});

	LOG_INFO << "=== End of Unclaimed Rewards Display ===";
}

// 统计未领取奖励的总数量
uint32_t RewardSystem::CountEntitiesWithUnclaimedRewards() {
	uint32_t count = 0;

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[&count](entt::entity /*entity*/, const RewardComp& rewardComp) {
			if (HasUnclaimedRewards(rewardComp.rewards)) {
				++count;
			}
		});

	return count;
}