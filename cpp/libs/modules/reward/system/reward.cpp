#include "reward.h"

#include "muduo/base/Logging.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include "reward/comp/reward.h"

#include <thread_context/registry_manager.h>
#include <sstream>

// --- Diagnostics (operate on RewardComp) ---

void RewardClaimSystem::ShowRewardStatus() {
	LOG_INFO << "=== Displaying Reward Status for All Entities ===";

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[](entt::entity entity, const RewardComp& rewardComp) {
			ShowEntityRewardStatus(rewardComp.claimedRewards);
		});

	LOG_INFO << "=== End of Reward Status Display ===";
}

void RewardClaimSystem::ShowEntityRewardStatus(const RewardBitset& claimedRewards) {
	for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
		const bool isClaimed = claimedRewards[i];
		LOG_INFO << "  Reward " << i << ": " << (isClaimed ? "Claimed" : "Not Claimed");
	}
}

void RewardClaimSystem::CountRewardStatistics() {
	uint64_t totalClaimed = 0;
	uint64_t totalRewards = 0;

	tlsRegistryManager.actorRegistry.view<RewardComp>().each([&](entt::entity /*entity*/, const RewardComp& rewardComp) {
		totalClaimed += static_cast<uint64_t>(rewardComp.claimedRewards.count());
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

void RewardClaimSystem::ShowUnclaimedRewards() {
	LOG_INFO << "=== Displaying Entities with Unclaimed Rewards ===";

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[](entt::entity entity, const RewardComp& rewardComp) {
			if (HasUnclaimed(rewardComp.claimedRewards)) {
				LOG_INFO << "Entity ID with unclaimed rewards: " << entt::to_integral(entity);
			}
		});

	LOG_INFO << "=== End of Unclaimed Rewards Display ===";
}

uint32_t RewardClaimSystem::CountEntitiesWithUnclaimedRewards() {
	uint32_t count = 0;

	tlsRegistryManager.actorRegistry.view<RewardComp>().each(
		[&count](entt::entity /*entity*/, const RewardComp& rewardComp) {
			if (HasUnclaimed(rewardComp.claimedRewards)) {
				++count;
			}
		});

	return count;
}
