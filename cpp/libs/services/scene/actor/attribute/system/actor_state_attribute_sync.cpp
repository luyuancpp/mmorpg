#include "actor_state_attribute_sync.h"

// 引入必要的头文件
#include "proto/service/rpc/scene/player_state_attribute_sync.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/actor_attribute_state_comp.pb.h"
#include "actor/attribute/constants/actor_state_attribute_constants.h"
#include "core/network/message_system.h"
#include "scene/scene/comp/scene_node_scene.h"
#include "scene/scene/system/view.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"

#include "frame/manager/frame_time.h"
#include "network/player_message_utils.h"
#include <threading/registry_manager.h>
#include <actor/attribute/comp/actor_attribute.h>
#include <generated/attribute/actorbaseattributess2c_attribute_sync.h>
#include <generated/attribute/attributedelta60framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta30framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta10framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta5framess2c_attribute_sync.h>
#include <generated/attribute/attributedelta2framess2c_attribute_sync.h>

// 定义帧同步频率的配置数组大小
constexpr uint32_t kSyncFrequencyArraySize = 5;

// 别名：定义同步频率数组类型
using SyncFrequencyArray = std::array<uint32_t, kSyncFrequencyArraySize>;

// 定义不同距离级别的同步频率
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

// 定义距离级别同步配置结构体，包含同步频率和获取实体列表的函数指针
struct DistanceSyncConfig {
	const SyncFrequencyArray& syncFrequencies;
	void (*retrieveEntityList)(const entt::entity, EntityVector&);
};

// 定义不同距离级别的同步配置
constexpr DistanceSyncConfig kDistanceSyncConfigs[] = {
	{kLevel1SyncFrequencies, ActorStateAttributeSyncSystem::GetNearbyLevel1Entities},
	{kLevel2SyncFrequencies, ActorStateAttributeSyncSystem::GetNearbyLevel2Entities},
	{kLevel3SyncFrequencies, ActorStateAttributeSyncSystem::GetNearbyLevel3Entities}
};

// 通用的同步函数，根据不同距离级别执行同步
void SyncAttributesForDistanceLevel(const entt::entity& entity, EntityVector& nearbyEntityList, const DistanceSyncConfig& distanceSyncConfig) {
	const auto currentFrame = tlsFrameTimeManager.frameTime.current_frame();

	// 获取该距离级别的实体列表
	distanceSyncConfig.retrieveEntityList(entity, nearbyEntityList);

	// 根据每个帧同步频率配置进行属性同步
	for (const auto& frequency : distanceSyncConfig.syncFrequencies) {
		// 添加检查，确保 frequency 大于 0
		if (frequency > 0 && currentFrame % frequency == 0) {
			ActorStateAttributeSyncSystem::SyncAttributes(entity, nearbyEntityList, frequency);
		}
	}

	// 清空实体列表，为下一个距离级别的同步做准备
	nearbyEntityList.clear();
}


// 系统更新函数，遍历每个实体并按距离级别进行属性同步
void ActorStateAttributeSyncSystem::Update(const double delta)
{
	EntityVector nearbyEntityList;

	for (auto [entity, transform] : tlsRegistryManager.actorRegistry.view<Transform>().each())
	{
		auto& actorRegistry = tlsRegistryManager.actorRegistry;
		const auto& aoiListComp = actorRegistry.get_or_emplace<AoiListComp>(entity);

		ActorBaseAttributesS2CSyncAttributes(entity, ScenePlayerSyncSyncBaseAttributeMessageId, aoiListComp.aoiList);

		// 处理各距离级别的同步，迭代 kDistanceSyncConfigs 数组，动态处理距离级别
		for (const auto& distanceSyncConfig : kDistanceSyncConfigs) {
			SyncAttributesForDistanceLevel(entity, nearbyEntityList, distanceSyncConfig);
		}
	}
}

// 初始化同步工具类，当前没有初始化逻辑
void ActorStateAttributeSyncSystem::Initialize() {
	// 可添加全局初始化逻辑
}

// 获取附近一级实体列表
void ActorStateAttributeSyncSystem::GetNearbyLevel1Entities(const entt::entity entity, EntityVector& nearbyEntities) {
	const auto& aoiList = tlsRegistryManager.actorRegistry.get_or_emplace<AoiListComp>(entity).aoiList;

	for (const auto& nearbyEntity : aoiList) {
		constexpr double viewRadiusFactor = 0.333;
		const double viewRadius = ViewSystem::GetMaxViewRadius(nearbyEntity) * viewRadiusFactor;

		if (!ViewSystem::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
			continue;
		}

		nearbyEntities.emplace_back(nearbyEntity);
	}
}

// 获取附近二级实体列表
void ActorStateAttributeSyncSystem::GetNearbyLevel2Entities(const entt::entity entity, EntityVector& nearbyEntities) {
	const auto& aoiList = tlsRegistryManager.actorRegistry.get_or_emplace<AoiListComp>(entity).aoiList;

	for (const auto& nearbyEntity : aoiList) {
		constexpr double viewRadiusFactor = 0.666;
		const double viewRadius = ViewSystem::GetMaxViewRadius(nearbyEntity) * viewRadiusFactor;

		if (!ViewSystem::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
			continue;
		}

		nearbyEntities.emplace_back(nearbyEntity);
	}
}

// 获取附近三级实体列表
void ActorStateAttributeSyncSystem::GetNearbyLevel3Entities(const entt::entity entity, EntityVector& nearbyEntities) {
	const auto& aoiList = tlsRegistryManager.actorRegistry.get_or_emplace<AoiListComp>(entity).aoiList;

	for (const auto& nearbyEntity : aoiList) {
		const double viewRadius = ViewSystem::GetMaxViewRadius(nearbyEntity);

		if (!ViewSystem::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
			continue;
		}

		nearbyEntities.emplace_back(nearbyEntity);
	}
}

// 同步属性，根据频率决定同步内容
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
