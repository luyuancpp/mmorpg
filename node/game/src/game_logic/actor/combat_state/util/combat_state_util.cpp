#include "combat_state_util.h"

#include "event/actor_combat_state_event.pb.h"
#include "game_logic/actor/combat_state/constants/combat_state_constants.h"
#include "proto/logic/component/actor_combat_state_comp.pb.h"
#include "thread_local/storage.h"

// 初始化实体的战斗状态组件
void CombatStateUtil::InitializeActorComponents(entt::entity entity) {
    tls.registry.emplace<CombatStateListPbComponent>(entity); // 添加战斗状态列表组件
}

// 处理添加战斗状态的逻辑
void CombatStateUtil::AddCombatState(const CombatStateAddedPbEvent& event) {
    const auto actorEntityId = entt::to_entity(event.actor_entity());
    auto& combatStateListComponent = tls.registry.get<CombatStateListPbComponent>(actorEntityId);

    // 检查状态类型是否合法
    if (event.state_type() >= kActorCombatStateMax) {
        return;
    }

    // 查找是否已存在对应的战斗状态
    auto stateIterator = combatStateListComponent.mutable_state_list()->find(event.state_type());
    if (stateIterator == combatStateListComponent.mutable_state_list()->end()) {
        // 如果不存在，插入新的战斗状态
        const auto [newStateIterator, inserted] = combatStateListComponent.mutable_state_list()->emplace(
            event.state_type(), CombatStatePbComponent{});
        if (!inserted) {
            return; // 插入失败，直接返回
        }
        stateIterator = newStateIterator;
    }

    // 将来源 buff ID 添加到状态的来源列表中
    stateIterator->second.mutable_sources()->emplace(event.source_buff_id(), false);
}

// 处理移除战斗状态的逻辑
void CombatStateUtil::RemoveCombatState(const CombatStateRemovedPbEvent& event) {
    const auto actorEntityId = entt::to_entity(event.actor_entity());
    auto& combatStateListComponent = tls.registry.get<CombatStateListPbComponent>(actorEntityId);

    // 检查状态类型是否合法
    if (event.state_type() >= kActorCombatStateMax) {
        return;
    }

    // 查找对应的战斗状态
    const auto stateIterator = combatStateListComponent.mutable_state_list()->find(event.state_type());
    if (stateIterator == combatStateListComponent.mutable_state_list()->end()) {
        return; // 如果状态不存在，直接返回
    }

    // 从状态的来源列表中移除对应的 buff ID
    stateIterator->second.mutable_sources()->erase(event.source_buff_id());
}
