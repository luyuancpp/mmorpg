#include "combat_state_system.h"

#include <ranges>

#include "table/actoractioncombatstate_table.h"
#include "proto/table/tip/common_error_tip.pb.h"
#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "actor/attribute/system/actor_attribute_calculator_system.h"
#include "scene//combat_state/constants/combat_state_constants.h"
#include "proto/logic/component/actor_combat_state_comp.pb.h"
#include "proto/logic/event/actor_combat_state_event.pb.h"

#include <threading/registry_manager.h>

// 初始化实体的战斗状态组件
void CombatStateSystem::InitializeCombatStateComponent(entt::entity entity) {
    tlsRegistryManager.actorRegistry.emplace<CombatStateCollectionPbComponent>(entity); // 添加战斗状态集合组件
}

// 添加战斗状态
void CombatStateSystem::AddCombatState(const CombatStateAddedPbEvent& addEvent) {
    // 获取实体 ID
    const auto entityId = entt::to_entity(addEvent.actor_entity());

    // 获取战斗状态集合组件
    auto& combatStateCollection = tlsRegistryManager.actorRegistry.get<CombatStateCollectionPbComponent>(entityId);

    // 检查状态类型是否合法
    if (addEvent.state_type() >= kActorMaxCombatStateType) {
        return; // 如果状态类型非法，直接返回
    }

    // 检查是否已存在该战斗状态
    auto stateIterator = combatStateCollection.mutable_states()->find(addEvent.state_type());
    if (stateIterator == combatStateCollection.mutable_states()->end()) {
        // 如果状态不存在，则创建新状态并插入集合
        const auto [newStateIterator, wasInserted] = combatStateCollection.mutable_states()->emplace(
            addEvent.state_type(), CombatStateDetailsPbComponent{});
        if (!wasInserted) {
            return; 
        }
        stateIterator = newStateIterator;
    }

    // 添加来源 Buff ID 到状态的来源列表中
    stateIterator->second.mutable_sources()->emplace(addEvent.source_buff_id(), false);

    // 标记需要更新战斗状态属性
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(entityId, kCombatState);
}

// 移除战斗状态
void CombatStateSystem::RemoveCombatState(const CombatStateRemovedPbEvent& removeEvent) {
    // 获取实体 ID
    const auto entityId = entt::to_entity(removeEvent.actor_entity());

    // 获取战斗状态集合组件
    auto& combatStateCollection = tlsRegistryManager.actorRegistry.get<CombatStateCollectionPbComponent>(entityId);

    // 检查状态类型是否合法
    if (removeEvent.state_type() >= kActorMaxCombatStateType) {
        return; // 如果状态类型非法，直接返回
    }

    // 查找对应的战斗状态
    auto stateIterator = combatStateCollection.mutable_states()->find(removeEvent.state_type());
    if (stateIterator == combatStateCollection.mutable_states()->end()) {
        return; // 如果状态不存在，直接返回
    }

    // 从状态的来源列表中移除来源 Buff ID
    stateIterator->second.mutable_sources()->erase(removeEvent.source_buff_id());

    // 如果来源列表为空，则移除该战斗状态
    if (stateIterator->second.sources().empty()) {
        combatStateCollection.mutable_states()->erase(stateIterator);
    }

    // 标记需要更新战斗状态属性
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(entityId, kCombatState);
}

uint32_t CombatStateSystem::ValidateSkillUsage(const entt::entity entityId, const uint32_t combatAction)
{
    // 获取实体的战斗状态集合
    const auto& combatStateCollection = tlsRegistryManager.actorRegistry.get<CombatStateCollectionPbComponent>(entityId);

    // 如果没有战斗状态，技能使用成功
    if (combatStateCollection.states().empty()) {
        return kSuccess;
    }

    FetchAndValidateActorActionCombatStateTable(combatAction);

    // 遍历所有战斗状态键，检查是否存在互斥状态
    for (const auto& stateKey : combatStateCollection.states() | std::views::keys) {
        // 获取当前状态对象
        const auto& combatState = actorActionCombatStateTable->state(stateKey);

        // 如果状态是互斥的，返回提示信息
        if (combatState.state_mode() == kCombatStateMutualExclusion) {
            return combatState.state_tip();
        }
    }

    // 所有检查通过，技能使用成功
    return kSuccess;
}
