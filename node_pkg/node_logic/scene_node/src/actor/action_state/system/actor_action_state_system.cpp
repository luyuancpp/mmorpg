#include "actor_action_state_system.h"

#include <ranges>

#include "actoractionstate_config.h"
#include "common_error_tip.pb.h"
#include "actor/action_state/constants/actor_state_constants.h"
#include "macros/return_define.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/event/actor_event.pb.h"
#include "thread_local/storage.h"
#include <thread_local/dispatcher_manager.h>

namespace {
    // 检查某个动作是否与当前状态冲突，若冲突，返回对应的错误码
    uint32_t CheckForStateConflict(const ActorActionStateTable* actorActionStateTable, uint32_t actorState) {
        if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
            return kSuccess;  // 如果状态无效，返回成功
        }

        if (const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateMutualExclusion) {
            return state.state_tip();  // 返回冲突的状态提示
        }
        return kSuccess;  // 没有冲突
    }

    // 中断当前状态并执行该动作
    bool InterruptAndPerformAction(const ActorActionStateTable* actorActionStateTable, uint32_t actorState, entt::entity actorEntity) {
        if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
            return false;  // 无效状态，返回失败
        }

        if (const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateInterrupt) {
            // 触发中断事件
            InterruptCurrentStatePbEvent interruptEvent;
            interruptEvent.set_actor_entity(entt::to_integral(actorEntity));
            interruptEvent.set_actor_state(actorState);
            dispatcher.trigger(interruptEvent);

            // 中断当前状态并执行该动作
            RETURN_FALSE_ON_ERROR(ActorActionStateSystem::RemoveState(actorEntity, actorState));
            
            return true;  // 成功中断并执行动作
        }

        return false;  // 不需要中断
    }
}

void ActorActionStateSystem::InitializeActorComponents(entt::entity entity) {
    // 初始化实体的状态组件
    tlsRegistryManager.actorRegistry.emplace<ActorStatePbComponent>(entity);
}

uint32_t ActorActionStateSystem::TryPerformAction(entt::entity actorEntity, uint32_t actorAction, uint32_t successState) {
    // 获取该动作对应的状态表
    FetchAndValidateActorActionStateTable(actorAction);

    // 遍历角色的所有状态，检查是否可以执行该动作
    const auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        // 检查该状态是否与动作冲突
        RETURN_ON_ERROR(CheckForStateConflict(actorActionStateTable, actorState));
    }

    // 检查并处理中断状态
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        if (InterruptAndPerformAction(actorActionStateTable, actorState, actorEntity)) {
            continue;  // 如果中断成功，继续检查下一个状态
        }
    }

    // 如果所有检查通过，可以添加状态并执行动作
    RETURN_ON_ERROR(AddState(actorEntity, successState));

    // 执行动作成功
    return kSuccess;
}


uint32_t ActorActionStateSystem::CanExecuteActionWithoutStateChange(entt::entity actorEntity, uint32_t actorAction) {
    FetchAndValidateActorActionStateTable(actorAction);
    
    // 获取角色状态组件并检查是否允许执行动作
    const auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        // 检查该状态是否与动作冲突
        RETURN_ON_ERROR(CheckForStateConflict(actorActionStateTable, actorState));
    }

    // 如果通过所有检查，则允许执行动作
    return kSuccess;
}

bool ActorActionStateSystem::HasState(const entt::entity actorEntity, const uint32_t state) {
    const auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get<ActorStatePbComponent>(actorEntity);
    if (state >= kActorStateActorStateMax) {
        return false;  // 无效状态
    }

    return actorStatePbComponent.state_list().contains(state);  // 检查是否包含指定状态
}

uint32_t ActorActionStateSystem::GetStateTip(const uint32_t actorAction, const uint32_t actorState) {
    FetchAndValidateActorActionStateTable(actorAction);

    if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
        return kInvalidParameter;  // 状态无效
    }

    const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
    return state.state_tip();  // 返回状态提示
}

uint32_t ActorActionStateSystem::AddState(const entt::entity actorEntity, uint32_t actorState) {
    auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax){
        return kInvalidParameter; 
    }

    if (actorStatePbComponent.state_list().contains(actorState)){
        return kSuccess;

    }

    // 添加新的状态
    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(actorState, true));
    return kSuccess;
}

uint32_t ActorActionStateSystem::RemoveState(entt::entity actorEntity, uint32_t actorState) {
    auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax ||
        !actorStatePbComponent.state_list().contains(actorState)) {
        return kInvalidParameter;
    }

    // 移除状态
    actorStatePbComponent.mutable_state_list()->erase(actorState);
    return kSuccess;
}
