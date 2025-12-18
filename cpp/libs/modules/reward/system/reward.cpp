#include "reward.h"

#include "muduo/base/Logging.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include "reward/comp/reward.h"

#include <threading/registry_manager.h>
#include <sstream>

uint32_t RewardClaimSystem::ClaimRewardByRewardId(uint32_t rewardId, RewardBitset& claimedRewards) {
	auto it = RewardBitMap.find(rewardId);
	if (it == RewardBitMap.end()) {
		return kInvalidTableId;
	}

	const auto rewardIndex = it->second;
	return ClaimRewardByIndex(rewardIndex, claimedRewards);
}

uint32_t RewardClaimSystem::ClaimRewardByIndex(uint32_t rewardIndex, RewardBitset& claimedRewards) {
	if (rewardIndex >= kRewardMaxBitIndex) {
		return kIndexOutOfRange;
	}

	if (claimedRewards[rewardIndex]) {
		return kRewardAlreadyClaimed;
	}

	claimedRewards.set(rewardIndex);  // 设置该奖励为已领取
	return kSuccess;
}

bool RewardClaimSystem::IsRewardClaimedById(uint32_t rewardId, const RewardBitset& claimedRewards)
{
	auto it = RewardBitMap.find(rewardId);
	if (it == RewardBitMap.end()) {
		return false;
	}

	const auto rewardIndex = it->second;
	return IsRewardClaimedByIndex(rewardIndex, claimedRewards);
}

bool RewardClaimSystem::IsRewardClaimedByIndex(uint32_t rewardIndex, const RewardBitset& claimedRewards) {
	if (rewardIndex >= kRewardMaxBitIndex) {
		return false;
	}

	return claimedRewards[rewardIndex];
}

void RewardClaimSystem::ShowRewardStatus() {
	LOG_INFO << "=== Displaying Reward Status for All Entities ===";

	// 获取所有拥有 RewardComp 的实体
	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[](entt::entity entity, const RewardComp& rewardComp) {
			ShowEntityRewardStatus(rewardComp.GlobalRewards());
		});

	LOG_INFO << "=== End of Reward Status Display ===";
}

// 显示单个实体的奖励状态
void RewardClaimSystem::ShowEntityRewardStatus(const RewardBitset& claimedRewards) {
	// 遍历所有奖励
	for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
		const bool isClaimed = claimedRewards[i];
		LOG_INFO << "  Reward " << i << ": " << (isClaimed ? "Claimed" : "Not Claimed");
	}
}

void RewardClaimSystem::CountRewardStatistics() {
	uint64_t totalClaimed = 0;
	uint64_t totalRewards = 0;

	tlsRegistryManager.actorRegistry.view<RewardComp>().each([&](entt::entity /*entity*/, const RewardComp& rewardComp) {
		// 如果 RewardBitset 是 std::bitset，可直接使用 count()
		totalClaimed += static_cast<uint64_t>(rewardComp.GlobalRewards().count());
		totalRewards += static_cast<uint64_t>(kRewardMaxBitIndex);
		});

	LOG_INFO << "Total Rewards: " << totalRewards
		<< ", Claimed: " << totalClaimed
		<< ", Unclaimed: " << (totalRewards - totalClaimed);
}

std::string RewardClaimSystem::FormatRewardStatus(entt::entity entityId, const RewardBitset& claimedRewards) {
	std::ostringstream ss;
	ss << "Entity ID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(entityId) << "\n";

	for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
		ss << "  Reward " << i << ": " << (claimedRewards[i] ? "Claimed" : "Not Claimed") << "\n";
	}

	return ss.str();
}

bool RewardClaimSystem::HasUnclaimedRewards(const RewardBitset& claimedRewards) {
	// 如果 RewardBitset 支持 count()
	if (claimedRewards.count() < kRewardMaxBitIndex) {
		return true;
	}
	return false;
}

// 例如仅显示未领取奖励的实体
void RewardClaimSystem::ShowUnclaimedRewards() {
	LOG_INFO << "=== Displaying Entities with Unclaimed Rewards ===";

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[](entt::entity entity, const RewardComp& rewardComp) {
			if (HasUnclaimedRewards(rewardComp.GlobalRewards())) {
				LOG_INFO << "Entity ID with unclaimed rewards: " << entt::to_integral(entity);
			}
		});

	LOG_INFO << "=== End of Unclaimed Rewards Display ===";
}

// 统计未领取奖励的总数量
uint32_t RewardClaimSystem::CountEntitiesWithUnclaimedRewards() {
	uint32_t count = 0;

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[&count](entt::entity /*entity*/, const RewardComp& rewardComp) {
			if (HasUnclaimedRewards(rewardComp.GlobalRewards())) {
				++count;
			}
		});

	return count;
}