#include "actor_action_state_util.h"
#include "actionstate_config.h"
#include "common_error_tip.pb.h"
#include "component/actor_comp.pb.h"
#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "thread_local/storage.h"

namespace
{
    // 检查某个动作是否是互斥的状态，若是，返回对应的错误码
    uint32_t CheckMutualExclusionState(const ActionStateTable* actionStateTable, uint32_t actorState)
    {
        if (actorState >= static_cast<uint32_t>(actionStateTable->state_size()))
        {
            return kSuccess;
        }

        if (const auto& state = actionStateTable->state(static_cast<int32_t>(actorState)); state.state_mode() == kActionStateMutualExclusion)
        {
            return state.state_tip();
        }
        return kSuccess;
    }

    // 中断当前状态并执行该动作
    bool InterruptCurrentStateAndExecuteAction(const ActionStateTable* actionStateTable, uint32_t actorState, entt::entity actorEntity)
    {
        if (actorState >= static_cast<uint32_t>(actionStateTable->state_size()))
        {
            return false;  // 无效状态
        }

        if (const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateInterruptAndExecute)
        {
            // 中断当前状态并执行该动作
            if (const uint32_t interruptResult = ActorActionStateUtil::ExitState(actorEntity, actorState);
                interruptResult != kSuccess)
            {
                return false;  // 退出失败，返回false
            }
        
            return true;  // 中断成功，允许执行动作
        }

        return false;  // 不需要中断
    }

}

void ActorActionStateUtil::InitializeActorComponents(entt::entity entity)
{
    // 初始化实体的状态组件
    tls.registry.emplace<ActorStatePbComponent>(entity);
}

uint32_t ActorActionStateUtil::TryToPerformAction(entt::entity actorEntity, uint32_t actorAction)
{
    // 获取该动作对应的状态表
    auto [actionStateTable, result] = GetActionStateTable(actorAction);
    if (nullptr == actionStateTable)
    {
        return result;  // 返回状态表错误码
    }

    // 遍历角色的所有状态，检查是否可以执行该动作
    const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys)
    {
        // 检查该状态是否与动作互斥
        if (const uint32_t exclusionResult = CheckMutualExclusionState(actionStateTable, actorState);
            exclusionResult != kSuccess)
        {
            return exclusionResult;
        }
    }
    
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys)
    {
        // 检查该动作是否需要中断当前状态并执行
        if (InterruptCurrentStateAndExecuteAction(actionStateTable, actorState, actorEntity))
        {
            // 如果当前状态被中断并执行新动作，退出状态检查
            continue;
        }
    }

    // 如果没有问题，可以执行动作
    return kSuccess;
}


uint32_t ActorActionStateUtil::CanPerformAction(entt::entity actorEntity, uint32_t actorAction)
{
    // 获取该动作对应的状态表
    auto [actionStateTable, result] = GetActionStateTable(actorAction);
    if (nullptr == actionStateTable)
    {
        return result;  // 返回状态表错误码
    }

    // 获取角色状态组件

    // 遍历角色当前所有状态，检查是否允许执行该动作
    for (const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
        const auto& actorState : actorStatePbComponent.state_list() | std::views::keys)
    {
        // 检查该状态是否与动作互斥
        if (const uint32_t exclusionResult = CheckMutualExclusionState(actionStateTable, actorState);
            exclusionResult != kSuccess)
        {
            return exclusionResult;  // 若状态互斥，则返回对应错误码
        }
    }

    // 如果通过所有检查，则允许执行动作
    return kSuccess;
}

bool ActorActionStateUtil::IsInState(entt::entity actorEntity, uint32_t state)
{
    // 检查角色是否处于指定的状态
    const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (state >= kActorStateActorStateMax)
    {
        return false;  // 无效的状态
    }

    return actorStatePbComponent.state_list().contains(state);
}

uint32_t ActorActionStateUtil::GetStatusCode(entt::entity actorEntity, uint32_t actorAction, uint32_t actorState)
{
    // 获取该动作对应的状态表
    auto [actionStateTable, result] = GetActionStateTable(actorAction);
    if (nullptr == actionStateTable)
    {
        return result;  // 返回状态表错误码
    }

    if (actorState >= static_cast<uint32_t>(actionStateTable->state_size()))
    {
        return kInvalidParameter;
    }

    const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
    
    return state.state_tip();
}

uint32_t ActorActionStateUtil::EnterState(entt::entity actorEntity, uint32_t actorState)
{
    auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax || actorStatePbComponent.state_list().contains(actorState))
    {
        return kInvalidParameter;  
    }
    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(actorState, true));
    return kSuccess;
}

uint32_t ActorActionStateUtil::ExitState(entt::entity actorEntity, uint32_t actorState)
{
    auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax || !actorStatePbComponent.state_list().contains(actorState))
    {
        return kInvalidParameter; 
    }
    actorStatePbComponent.mutable_state_list()->erase(actorState);
    return kSuccess;
}

uint32_t ActorActionStateUtil::InterruptCurrentAction(entt::entity actorEntity, uint32_t actorAction)
{
    return kSuccess;
}