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
            return kOK;
        }

        const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
        if (state.state_flag() == kActionStateMutualExclusion)
        {
            return state.state_tip();
        }
        return kOK;
    }

    // 检查某个动作是否允许，若允许，继续执行；若有中断标志，则跳过
    bool ShouldInterruptAction(const ActionStateTable* actionStateTable, uint32_t actorState)
    {
        if (actorState >= static_cast<uint32_t>(actionStateTable->state_size()))
        {
            return false;
        }

        const auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
        return state.state_flag() == kActionStateInterruptAndExecute;
    }
}

void ActorActionStateUtil::InitializeActorComponents(entt::entity entity)
{
    // 初始化实体的状态组件
    tls.registry.emplace<ActorStatePbComponent>(entity);
}

uint32_t ActorActionStateUtil::TryToPerformAction(entt::entity actorEntity, uint32_t action)
{
    // 获取该动作对应的状态表
    auto [actionStateTable, result] = GetActionStateTable(action);
    if (nullptr == actionStateTable)
    {
        return result;  // 返回状态表错误码
    }

    // 遍历角色的所有状态，检查是否可以执行该动作
    const auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys)
    {
        // 检查该状态是否与动作互斥
        uint32_t exclusionResult = CheckMutualExclusionState(actionStateTable, actorState);
        if (exclusionResult != kOK)
        {
            return exclusionResult;
        }

        // 检查该动作是否需要中断执行，若是，则跳过
        if (ShouldInterruptAction(actionStateTable, actorState))
        {
            continue;
        }
    }

    return kOK;  // 如果没有问题，可以执行动作
}

uint32_t ActorActionStateUtil::CanPerformAction(entt::entity actorEntity, uint32_t action)
{
    // 暂时返回 kOK，表示允许执行动作。可以在未来扩展更多的检查逻辑。
    return kOK;
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

uint32_t ActorActionStateUtil::EnterState(entt::entity actorEntity, uint32_t state)
{
    // 检查角色是否已经处于指定状态，若已在该状态中，则无须再次进入
    auto& actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    if (state >= kActorStateActorStateMax || actorStatePbComponent.state_list().contains(state))
    {
        return kInvalidParameter;  // 错误的状态参数
    }

    // 进入新状态
    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(state, true));
    return kOK;
}

uint32_t ActorActionStateUtil::InterruptCurrentAction(entt::entity actorEntity, uint32_t state)
{
    // 目前没有明确的中断逻辑，返回 kOK
    return kOK;
}
