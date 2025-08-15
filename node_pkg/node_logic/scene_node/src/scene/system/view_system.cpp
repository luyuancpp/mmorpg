#include "view_system.h"

#include "grid_system.h"
#include "proto/logic/component/comp.pb.h"
#include "Detour/DetourCommon.h"
#include "core/network/message_system.h"
#include "scene/constants/view_constants.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "thread_local/storage.h"
#include "type_define/type_define.h"
#include "network/player_message_utils.h"

void ViewSystem::Initialize()
{
	// Initialize actor creation and destruction messages in the global registry
	InitializeActorMessages();
}

void ViewSystem::InitializeActorMessages()
{
	tls.globalRegistry.emplace<ActorCreateS2C>(GlobalEntity());
	tls.globalRegistry.emplace<ActorDestroyS2C>(GlobalEntity());
	tls.globalRegistry.emplace<ActorListCreateS2C>(GlobalEntity());
	tls.globalRegistry.emplace<ActorListDestroyS2C>(GlobalEntity());
}

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
	if (BothAreNpcs(observer, entrant)) {
		return false;
	}

	if (EntrantIsNpc(entrant)) {
		return true;
	}

	// Handle cases where sudden loss of visibility requires refreshing view
	return ShouldRefreshView();
}

bool ViewSystem::BothAreNpcs(entt::entity observer, entt::entity entrant)
{
	return tls.actorRegistry.any_of<Npc>(observer) && tls.actorRegistry.any_of<Npc>(entrant);
}

bool ViewSystem::EntrantIsNpc(entt::entity entrant)
{
	return tls.actorRegistry.any_of<Npc>(entrant);
}

bool ViewSystem::ShouldRefreshView()
{
	// TODO: Implement logic for when view needs refreshing
	return true;
}

double ViewSystem::GetMaxViewRadius(entt::entity observer)
{
	double viewRadius = kMaxViewRadius;

	if (const auto observerViewRadius = tls.actorRegistry.try_get<ViewRadius>(observer)) {
		viewRadius = observerViewRadius->radius();
	}

	return viewRadius;
}

bool ViewSystem::IsWithinViewRadius(entt::entity viewer, entt::entity targetEntity, double visionRadius)
{
	const auto viewerTransform = tls.actorRegistry.try_get<Transform>(viewer);
	const auto targetTransform = tls.actorRegistry.try_get<Transform>(targetEntity);

	// 如果缺少位置数据，返回 false，表示不在视野内
	if (!viewerTransform || !targetTransform) {
		return false;
	}

	// 获取观察者和目标实体的位置
	const dtReal viewerLocation[] = {
		viewerTransform->location().x(),
		viewerTransform->location().y(),
		viewerTransform->location().z()
	};
	const dtReal targetLocation[] = {
		targetTransform->location().x(),
		targetTransform->location().y(),
		targetTransform->location().z()
	};

	// 计算实体间的距离，并检查是否在视野范围内
	return dtVdist(viewerLocation, targetLocation) <= visionRadius;
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant)
{
	const double viewRadius = GetMaxViewRadius(observer);

	return IsWithinViewRadius(observer, entrant, viewRadius);
}

double ViewSystem::GetDistanceBetweenEntities(entt::entity entity1, entt::entity entity2)
{
	const auto transform1 = tls.actorRegistry.try_get<Transform>(entity1);
	const auto transform2 = tls.actorRegistry.try_get<Transform>(entity2);

	// 如果任一实体缺少位置数据，返回 -1 表示距离不可计算
	if (!transform1 || !transform2) {
		return -1.0;
	}

	// 获取两个实体的位置
	const dtReal location1[] = {
		transform1->location().x(),
		transform1->location().y(),
		transform1->location().z()
	};
	const dtReal location2[] = {
		transform2->location().x(),
		transform2->location().y(),
		transform2->location().z()
	};

	// 计算并返回两者之间的距离
	return dtVdist(location1, location2);
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entrant, ActorCreateS2C& createMessage)
{
	createMessage.set_entity(entt::to_integral(entrant));

	if (const auto entrantTransform = tls.actorRegistry.try_get<Transform>(entrant)) {
		createMessage.mutable_transform()->CopyFrom(*entrantTransform);
	}

	/*if (const auto guid = tls.registry.try_get<Guid>(entrant)) {
		createMessage.set_guid(*guid);
	}*/
}

void ViewSystem::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
	// Placeholder for handling player leave message
	// Specific logic can be added based on requirements
}

void ViewSystem::BroadcastToNearbyEntities(entt::entity entity, const uint32_t message_id,
const google::protobuf::Message& message, bool excludingSel)
{
	EntityUnorderedSet entites;
	GridSystem::GetEntitiesInGridAndNeighbors(entity, entites, excludingSel);
	BroadcastMessageToPlayers(message_id, message, entites);
}

void ViewSystem::BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
	const google::protobuf::Message& message)
{
	EntityUnorderedSet entites;
	GridSystem::GetEntitiesInViewAndNearby(entity, entites);
	BroadcastMessageToPlayers(message_id, message, entites);
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3& pos) {
    auto transform = tls.actorRegistry.try_get<Transform>(entity);
	if (nullptr == transform)
	{
		return;
	}

    // 计算目标方向
    dtReal targetLocation[] = { pos.x(), pos.y(), pos.z() };
    dtReal location[] = { transform->location().x(), transform->location().y(), transform->location().z() };
    dtReal direction[3] = { 0, 0, 0 };
    dtVsub(direction, targetLocation, location);

    // 归一化方向向量
    dtVnormalize(direction);

    // 检查方向向量的有效性
    if (direction[0] == 0.0 && direction[1] == 0.0 && direction[2] == 0.0) {
        return; // 如果方向向量为零，退出函数
    }

    // 计算旋转的欧拉角
    float yaw = atan2(direction[0], direction[2]); // 计算绕Y轴的旋转
    float pitch = asin(direction[1]); // 计算绕X轴的旋转

    // 更新 rotation 为欧拉角（以弧度为单位）
    transform->mutable_rotation()->set_x(pitch);
    transform->mutable_rotation()->set_y(yaw);
    transform->mutable_rotation()->set_z(0); // Z轴旋转保持为0
}
