#include "actor_state_attribute_sync.h"

#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/actor_attribute_state_comp.pb.h"
#include "actor/attribute/constants/actor_state_attribute_constants.h"
#include "core/network/message_system.h"
#include "spatial/comp/scene_node_scene_comp.h"
#include "spatial/system/view.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"

#include "frame/manager/frame_time.h"
#include "network/player_message_utils.h"
#include <thread_context/registry_manager.h>
#include <actor/attribute/comp/actor_attribute_comp.h>
#include <generated/attribute/actorbaseattributess2c_attribute_sync.h>
#include <generated/attribute/attributedelta60framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta30framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta10framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta5framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta2framess2c_attribute_sync.h>

constexpr uint32_t kSyncFrequencyArraySize = 5;

using SyncFrequencyArray = std::array<uint32_t, kSyncFrequencyArraySize>;

// Sync frequencies per distance level
constexpr SyncFrequencyArray kLevel1SyncFrequencies{
	eAttributeSyncFrequency::kSyncEvery2Frames,
	eAttributeSyncFrequency::kSyncEvery5Frames,
	eAttributeSyncFrequency::kSyncEvery10Frames,
	eAttributeSyncFrequency::kSyncEvery30Frames,
	eAttributeSyncFrequency::kSyncEvery60Frames };

constexpr SyncFrequencyArray kLevel2SyncFrequencies{
	eAttributeSyncFrequency::kSyncEvery2Frames,
	eAttributeSyncFrequency::kSyncEvery5Frames,
	eAttributeSyncFrequency::kSyncEvery10Frames };

constexpr SyncFrequencyArray kLevel3SyncFrequencies{
	eAttributeSyncFrequency::kSyncEvery2Frames };

// Distance-level sync config: sync frequencies + entity list retrieval function
struct DistanceSyncConfig {
	const SyncFrequencyArray& syncFrequencies;
	void (*retrieveEntityList)(const entt::entity, EntityVector&);
};

constexpr DistanceSyncConfig kDistanceSyncConfigs[] = {
	{kLevel1SyncFrequencies, ActorStateAttributeSyncSystem::GetNearbyLevel1Entities},
	{kLevel2SyncFrequencies, ActorStateAttributeSyncSystem::GetNearbyLevel2Entities},
	{kLevel3SyncFrequencies, ActorStateAttributeSyncSystem::GetNearbyLevel3Entities}
};

// Sync attributes for a given distance level
void SyncAttributesForDistanceLevel(const entt::entity& entity, EntityVector& nearbyEntityList, const DistanceSyncConfig& distanceSyncConfig) {
	const auto currentFrame = tlsFrameTimeManager.frameTime.current_frame();

	distanceSyncConfig.retrieveEntityList(entity, nearbyEntityList);

	for (const auto& frequency : distanceSyncConfig.syncFrequencies) {
		if (frequency > 0 && currentFrame % frequency == 0) {
			ActorStateAttributeSyncSystem::SyncAttributes(entity, nearbyEntityList, frequency);
		}
	}

	nearbyEntityList.clear();
}


void ActorStateAttributeSyncSystem::Update(const double delta)
{
	EntityVector nearbyEntityList;

	for (auto [entity, transform] : tlsEcs.actorRegistry.view<Transform>().each())
	{
		auto& actorRegistry = tlsEcs.actorRegistry;
		const auto& aoiListComp = actorRegistry.get_or_emplace<AoiListComp>(entity);

		ActorBaseAttributesS2CSyncAttributes(entity, ScenePlayerSyncSyncBaseAttributeMessageId, aoiListComp.GetEntitySet());

		for (const auto& distanceSyncConfig : kDistanceSyncConfigs) {
			SyncAttributesForDistanceLevel(entity, nearbyEntityList, distanceSyncConfig);
		}
	}
}

void ActorStateAttributeSyncSystem::GetNearbyLevel1Entities(const entt::entity entity, EntityVector& nearbyEntities) {
	const auto& aoiEntries = tlsEcs.actorRegistry.get_or_emplace<AoiListComp>(entity).entries;

	for (const auto& [nearbyEntity, _] : aoiEntries) {
		constexpr double viewRadiusFactor = 0.333;
		const double viewRadius = ViewSystem::GetMaxViewRadius(nearbyEntity) * viewRadiusFactor;

		if (!ViewSystem::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
			continue;
		}

		nearbyEntities.emplace_back(nearbyEntity);
	}
}

void ActorStateAttributeSyncSystem::GetNearbyLevel2Entities(const entt::entity entity, EntityVector& nearbyEntities) {
	const auto& aoiEntries = tlsEcs.actorRegistry.get_or_emplace<AoiListComp>(entity).entries;

	for (const auto& [nearbyEntity, _] : aoiEntries) {
		constexpr double viewRadiusFactor = 0.666;
		const double viewRadius = ViewSystem::GetMaxViewRadius(nearbyEntity) * viewRadiusFactor;

		if (!ViewSystem::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
			continue;
		}

		nearbyEntities.emplace_back(nearbyEntity);
	}
}

void ActorStateAttributeSyncSystem::GetNearbyLevel3Entities(const entt::entity entity, EntityVector& nearbyEntities) {
	const auto& aoiEntries = tlsEcs.actorRegistry.get_or_emplace<AoiListComp>(entity).entries;

	for (const auto& [nearbyEntity, _] : aoiEntries) {
		const double viewRadius = ViewSystem::GetMaxViewRadius(nearbyEntity);

		if (!ViewSystem::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
			continue;
		}

		nearbyEntities.emplace_back(nearbyEntity);
	}
}

void ActorStateAttributeSyncSystem::SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency) {
    if (nearbyEntities.empty()) return;

    switch (syncFrequency) {
    case eAttributeSyncFrequency::kSyncEvery2Frames: {
		AttributeDelta2FramesS2CSyncAttributes(entity, ScenePlayerSyncSyncAttribute2FramesMessageId, nearbyEntities);
        break;
    }
    case eAttributeSyncFrequency::kSyncEvery5Frames: {
		AttributeDelta5FramesS2CSyncAttributes(entity, ScenePlayerSyncSyncAttribute5FramesMessageId, nearbyEntities);
        break;
    }
    case eAttributeSyncFrequency::kSyncEvery10Frames: {
		AttributeDelta10FramesS2CSyncAttributes(entity, ScenePlayerSyncSyncAttribute10FramesMessageId, nearbyEntities);
        break;
    }
    case eAttributeSyncFrequency::kSyncEvery30Frames: {
		AttributeDelta30FramesS2CSyncAttributes(entity, ScenePlayerSyncSyncAttribute30FramesMessageId, nearbyEntities);
        break;
    }
    case eAttributeSyncFrequency::kSyncEvery60Frames: {
		AttributeDelta60FramesS2CSyncAttributes(entity, ScenePlayerSyncSyncAttribute60FramesMessageId, nearbyEntities);
        break;
    }
    default:
        break;
    }
}

