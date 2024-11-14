#include "actor_action_state_util.h"
#include "actionstate_config.h"
#include "common_error_tip.pb.h"
#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "macros/return_define.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/event/actor_event.pb.h"
#include "thread_local/storage.h"

namespace {
    // 检查某个动作是否与当前状态冲突，若冲突，返回对应的错误码
    uint32_t CheckForStateConflict(const ActionStateTable* actionStateTable, uint32_t actorState) {
        if (actorState >= static_cast<uint32_t>(actionStateTable->state_size())) {
            return kSuccess;  // 如果状态无效，返回成功
        }

        const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
        if (state.state_mode() == kActionStateMutualExclusion) {
            return state.state_tip();  // 返回冲突的状态提示
        }
        return kSuccess;  // 没有冲突
    }

    // 中断当前状态并执行该动作
    bool InterruptAndPerformAction(const ActionStateTable* actionStateTable, uint32_t actorState, entt::entity actorEntity) {
        if (actorState >= static_cast<uint32_t>(actionStateTable->state_size())) {
            return false;  // 无效状态，返回失败
        }

        const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
        if (state.state_mode() == kActionStateInterrupt) {
            // 触发中断事件
            InterruptCurrentStatePbEvent interruptEvent;
            interruptEvent.set_actor_entity(entt::to_integral(actorEntity));
            interruptEvent.set_actor_state(actorState);
            tls.dispatcher.trigger(interruptEvent);

            // 中断当前状态并执行该动作
            const uint32_t interruptResult = ActorActionStateUtil::RemoveState(actorEntity, actorState);
            if (interruptResult != kSuccess) {
                return false;  // 中断失败，返回失败
            }
            return true;  // 成功中断并执行动作
        }

        return false;  // 不需要中断
    }
}

void ActorActionStateUtil::InitializeActorComponents(entt::entity entity) {
    // 初始化实体的状态组件
    tls.registry.emplace<ActorStatePbComponent>(entity);
}

uint32_t ActorActionStateUtil::AddStateForAction(entt::entity actorEntity, uint32_t actorState) {
    // 调用 AddState 来添加状态
    RETURN_ON_ERROR(AddState(actorEntity, actorState));

    // 你可以在这里添加其他逻辑，例如日志记录等
    return kSuccess;
}

uint32_t ActorActionStateUtil::TryPerformAction(entt::entity actorEntity, uint32_t actorAction) {
    // 获取该动作对应的状态表
    FetchAndValidateActionStateTable(actorAction);
    
    // 遍历角色的所有状态，检查是否可以执行该动作
    const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        // 检查该状态是否与动作冲突
        RETURN_ON_ERROR(CheckForStateConflict(actionStateTable, actorState));
    }

    // 检查并处理中断状态
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        if (InterruptAndPerformAction(actionStateTable, actorState, actorEntity)) {
            continue;  // 如果中断成功，继续检查下一个状态
        }
    }

    // 如果所有检查通过，可以添加状态并执行动作
    RETURN_ON_ERROR(AddStateForAction(actorEntity, actorAction));

    // 执行动作成功
    return kSuccess;
}

uint32_t ActorActionStateUtil::CanExecuteActionWithoutStateChange(entt::entity actorEntity, uint32_t actorAction) {
    FetchAndValidateActionStateTable(actorAction);
    
    // 获取角色状态组件并检查是否允许执行动作
    const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        // 检查该状态是否与动作冲突
        RETURN_ON_ERROR(CheckForStateConflict(actionStateTable, actorState));
    }

    // 如果通过所有检查，则允许执行动作
    return kSuccess;
}

bool ActorActionStateUtil::HasState(const entt::entity actorEntity, const uint32_t state) {
    const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (state >= kActorStateActorStateMax) {
        return false;  // 无效状态
    }

    return actorStatePbComponent.state_list().contains(state);  // 检查是否包含指定状态
}

uint32_t ActorActionStateUtil::GetStateTip(const uint32_t actorAction, const uint32_t actorState) {
    FetchAndValidateActionStateTable(actorAction);

    if (actorState >= static_cast<uint32_t>(actionStateTable->state_size())) {
        return kInvalidParameter;  // 状态无效
    }

    const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
    return state.state_tip();  // 返回状态提示
}

uint32_t ActorActionStateUtil::AddState(const entt::entity actorEntity, uint32_t actorState) {
    auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax || actorStatePbComponent.state_list().contains(actorState)) {
        return kInvalidParameter;  // 状态无效或已存在
    }

    // 添加新的状态
    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(actorState, true));
    return kSuccess;
}

uint32_t ActorActionStateUtil::RemoveState(entt::entity actorEntity, uint32_t actorState) {
    auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax || !actorStatePbComponent.state_list().contains(actorState)) {
        return kInvalidParameter;  // 状态无效或不存在
    }

    // 移除状态
    actorStatePbComponent.mutable_state_list()->erase(actorState);
    return kSuccess;
}
