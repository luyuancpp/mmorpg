#include "game_logic/scene/util/view_util.h"
#include "logic/component/actor_comp.pb.h"

#include "type_define/type_define.h"
#include "thread_local/storage.h"

extern EntityUnorderedMap entitiesToNotifyEntry;
extern EntityUnorderedMap entitiesToNotifyExit;

bool ViewUtil::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    return true; // Mock behavior
}

bool ViewUtil::ShouldUpdateView(entt::entity observer, entt::entity entity) {
    
    return true; // Mock behavior
}

void ViewUtil::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage) {
    entitiesToNotifyEntry.emplace(observer, entity);
}

void ViewUtil::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
    entitiesToNotifyExit.emplace(observer, leaver);
}

bool ViewUtil::IsWithinViewRadius(entt::entity observer, entt::entity entrant, double view_radius)
{
    return true;
}

double ViewUtil::GetMaxViewRadius(entt::entity observer)
{
    return 0;
}

void ViewUtil::LookAtPosition(entt::entity entity, const Vector3& pos) {
    // 获取实体的 Transform 组件
    auto& transform = tls.registry.get<Transform>(entity);

    // 计算目标方向
    glm::vec3 direction = glm::normalize(glm::vec3(pos.x(), pos.y(), pos.z()) - transform.location().x());
    
    // 计算旋转的欧拉角
    float yaw = atan2(direction.x, direction.z); // 计算绕Y轴的旋转
    float pitch = asin(direction.y); // 计算绕X轴的旋转

    // 更新 rotation 为欧拉角（以弧度为单位）
    transform.rotation = glm::vec3(pitch, yaw, 0.0f); // 假设Z轴旋转为0
}

