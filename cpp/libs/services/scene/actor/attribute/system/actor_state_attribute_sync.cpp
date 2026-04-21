#include "actor_state_attribute_sync.h"

#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "actor/attribute/constants/actor_state_attribute_constants.h"
#include "spatial/comp/scene_node_scene_comp.h"
#include "spatial/system/view.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"

#include "frame/manager/frame_time.h"
#include "network/player_message_utils.h"
#include <generated/attribute/actorbaseattributess2c_attribute_sync.h>
#include <generated/attribute/attributedelta60framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta30framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta10framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta5framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta2framess2c_attribute_sync.h>
#include <thread_context/ecs_context.h>

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

constexpr double kLevel1RadiusFactor = 0.333;
constexpr double kLevel2RadiusFactor = 0.666;

// Sync entities at the appropriate frequencies for this frame
static void SyncEntitiesAtFrequencies(entt::entity entity, const EntityVector& entities,
									  const SyncFrequencyArray& frequencies, uint32_t currentFrame)
{
	if (entities.empty()) return;
	for (const auto& frequency : frequencies) {
		if (frequency > 0 && currentFrame % frequency == 0) {
			ActorStateAttributeSyncSystem::SyncAttributes(entity, entities, frequency);
		}
	}
}

void ActorStateAttributeSyncSystem::Update(const double delta)
{
	const auto currentFrame = tlsFrameTimeManager.frameTime.current_frame();
	EntityVector allAoiEntities, level1Entities, level2Entities, level3Entities;

	for (auto [entity, transform] : tlsEcs.actorRegistry.view<Transform>().each())
	{
		const auto* aoiListComp = tlsEcs.actorRegistry.try_get<AoiListComp>(entity);
		if (!aoiListComp || aoiListComp->entries.empty()) continue;

		// Build flat entity list for base attribute sync (avoids hash set allocation)
		allAoiEntities.clear();
		allAoiEntities.reserve(aoiListComp->entries.size());
		for (const auto& [e, _] : aoiListComp->entries) {
			allAoiEntities.emplace_back(e);
		}

		ActorBaseAttributesS2CSyncAttributes(entity, ScenePlayerSyncSyncBaseAttributeMessageId, allAoiEntities);

		// Single-pass distance classification: compute distance once per neighbor
		level1Entities.clear();
		level2Entities.clear();
		level3Entities.clear();

		for (const auto& [nearbyEntity, _] : aoiListComp->entries) {
			const double maxRadius = ViewSystem::GetMaxViewRadius(nearbyEntity);
			const auto distanceOpt = ViewSystem::GetDistanceBetweenEntities(nearbyEntity, entity);
			if (!distanceOpt)
				continue;
			const double distance = *distanceOpt;

			if (distance <= maxRadius * kLevel1RadiusFactor) {
				level1Entities.emplace_back(nearbyEntity);
			} else if (distance <= maxRadius * kLevel2RadiusFactor) {
				level2Entities.emplace_back(nearbyEntity);
			} else if (distance <= maxRadius) {
				level3Entities.emplace_back(nearbyEntity);
			}
		}

		SyncEntitiesAtFrequencies(entity, level1Entities, kLevel1SyncFrequencies, currentFrame);
		SyncEntitiesAtFrequencies(entity, level2Entities, kLevel2SyncFrequencies, currentFrame);
		SyncEntitiesAtFrequencies(entity, level3Entities, kLevel3SyncFrequencies, currentFrame);
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

