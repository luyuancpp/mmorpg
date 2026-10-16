#include "actor_action_state.h"

#include <ranges>

#include "table/code/actoractionstate_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "actor/action_state/constants/actor_state.h"
#include "engine/core/macros/return_define.h"
#include "engine/core/macros/error_return.h"
#include "engine/core/type_define/type_define.h"
#include "proto/common/component/actor_comp.pb.h"

namespace {
    // Check if an action conflicts with the current state; return error code on conflict
    uint32_t CheckForStateConflict(const ActorActionStateTable* actorActionStateTable, uint32_t actorState) {
        if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
            return kSuccess;
        }

        if (const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateMutualExclusion) {
            return state.state_tip();
        }
        return kSuccess;
    }

    // Interrupt current state and perform the action
    bool InterruptAndPerformAction(const ActorActionStateTable* actorActionStateTable, uint32_t actorState, entt::entity actorEntity) {
        if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
            return false;
        }

        if (const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateInterrupt) {
            RETURN_FALSE_ON_ERROR(ActorActionStateSystem::RemoveState(actorEntity, actorState));
            
            return true;
        }

        return false;
    }

    // 行为状态按需存在:没有组件表示尚无激活状态,查询不能替实体创建空列表。
    ActorStateComp* FindActorStateComp(entt::entity actorEntity) {
        if (!tlsEcs.actorRegistry.valid(actorEntity)) {
            return nullptr;
        }
        return tlsEcs.actorRegistry.try_get<ActorStateComp>(actorEntity);
    }
}

uint32_t ActorActionStateSystem::TryPerformAction(entt::entity actorEntity, uint32_t actorAction, uint32_t successState) {
    LookupActorActionStateOrReturnError(actorAction);

    // 校验前置:实体与目标状态值都确认无误之后,才允许触发打断事件、删改状态。
    // 以前非法 successState 要等打断都做完、到 AddState 里才被拒,前面的打断已经生效。
    if (!tlsEcs.actorRegistry.valid(actorEntity))
    {
        return MAKE_ERROR_MSG(kThisEntityIsInvalid,
            "entity=" << entt::to_integral(actorEntity)
            << " actorAction=" << actorAction);
    }
    if (successState >= kActorStateActorStateMax)
    {
        return MAKE_ERROR_MSG(kInvalidParameter,
            "entity=" << entt::to_integral(actorEntity)
            << " successState=" << successState);
    }
    // 打断分支会走 RemoveState -> state_list()->erase(actorState),而 protobuf Map
    // 的 erase 会让指向被删元素的迭代器失效 —— 正好就是 range-for 手里那一个,
    // 下一次 ++it 就是 UB。这条路径挂在每次释放技能的 CheckState 上,必须先把
    // 状态键快照出来再遍历。快照同时也切断了对 actorStatePbComponent 指针的依赖
    // (RemoveState 会修改对应组件里的状态表)。
    UInt32Vector currentStates;
    if (const auto *actorStatePbComponent = FindActorStateComp(actorEntity))
    {
        currentStates.reserve(actorStatePbComponent->state_list().size());
        for (const auto &actorState : actorStatePbComponent->state_list() | std::views::keys)
        {
            RETURN_ON_ERROR(CheckForStateConflict(actorActionStateRow, actorState));
            currentStates.emplace_back(actorState);
        }
    }

    for (const auto actorState : currentStates)
    {
        InterruptAndPerformAction(actorActionStateRow, actorState, actorEntity);
    }

    RETURN_ON_ERROR(AddState(actorEntity, successState));

    return kSuccess;
}


uint32_t ActorActionStateSystem::CanExecuteActionWithoutStateChange(entt::entity actorEntity, uint32_t actorAction) {
    LookupActorActionStateOrReturnError(actorAction);

    if (!tlsEcs.actorRegistry.valid(actorEntity))
    {
        return MAKE_ERROR_MSG(kThisEntityIsInvalid,
            "entity=" << entt::to_integral(actorEntity)
            << " actorAction=" << actorAction);
    }
    const auto *actorStatePbComponent = FindActorStateComp(actorEntity);
    if (actorStatePbComponent == nullptr)
    {
        return kSuccess;
    }
    for (const auto &actorState : actorStatePbComponent->state_list() | std::views::keys)
    {
        RETURN_ON_ERROR(CheckForStateConflict(actorActionStateRow, actorState));
    }

    return kSuccess;
}

bool ActorActionStateSystem::HasState(const entt::entity actorEntity, const uint32_t state) {
    if (state >= kActorStateActorStateMax) {
        return false;
    }

    const auto *actorStatePbComponent = FindActorStateComp(actorEntity);
    return actorStatePbComponent != nullptr && actorStatePbComponent->state_list().contains(state);
}

uint32_t ActorActionStateSystem::AddState(const entt::entity actorEntity, uint32_t actorState) {
    if (actorState >= kActorStateActorStateMax){
        return MAKE_ERROR_MSG(kInvalidParameter,
            "entity=" << entt::to_integral(actorEntity)
            << " actorState=" << actorState);
    }

    if (!tlsEcs.actorRegistry.valid(actorEntity))
    {
        return MAKE_ERROR_MSG(kThisEntityIsInvalid,
            "entity=" << entt::to_integral(actorEntity)
            << " actorState=" << actorState);
    }

    // 唯一写入口在第一次激活状态时延迟创建,只读校验不创建。
    auto& actorStatePbComponent = tlsEcs.actorRegistry.get_or_emplace<ActorStateComp>(actorEntity);
    if (actorStatePbComponent.state_list().contains(actorState)){
        return kSuccess;
    }

    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(actorState, true));
    return kSuccess;
}

uint32_t ActorActionStateSystem::RemoveState(entt::entity actorEntity, uint32_t actorState) {
    if (!tlsEcs.actorRegistry.valid(actorEntity))
    {
        return MAKE_ERROR_MSG(kThisEntityIsInvalid,
                              "entity=" << entt::to_integral(actorEntity)
                                        << " actorState=" << actorState);
    }
    if (actorState >= kActorStateActorStateMax)
    {
        return MAKE_ERROR_MSG(kInvalidParameter,
            "entity=" << entt::to_integral(actorEntity)
            << " actorState=" << actorState);
    }

    // 没有列表或目标状态都表示已经移除;幂等清理不创建组件。
    if (auto *actorStatePbComponent = FindActorStateComp(actorEntity)) {
        actorStatePbComponent->mutable_state_list()->erase(actorState);
    }
    return kSuccess;
}

