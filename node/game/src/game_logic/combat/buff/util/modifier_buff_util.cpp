#include "modifier_buff_util.h"
#include "buff_config.h"
#include "buff_config.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_calculator_constants.h"
#include "game_logic/actor/util/actor_attribute_calculator_util.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"

bool ModifierBuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    // 检查 buffTable 是否有效
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorUtil::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

void ModifierBuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp) {
    // 当前未实现，可以添加逻辑来刷新 buff
}

bool ModifierBuffUtil::OnBuffRemove(entt::entity parent, uint64_t buffId) {
    auto& buffList = tls.registry.get<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    // 查找 buff
    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return false;
    }

    auto [buffTable, result] = GetBuffTable(buffIt->second.buffPb.buff_table_id());
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorUtil::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

bool ModifierBuffUtil::OnBuffDestroy(entt::entity parent, uint32_t buffTableId) {
    // 当前未实现，可以添加销毁 buff 的逻辑
    return false;
}

void ModifierBuffUtil::ModifyState(entt::entity parent, uint64_t buffId) {
    // 当前未实现，可以添加状态修改的逻辑
}

void ModifierBuffUtil::ModifyAttributes(entt::entity parent, uint64_t buffId) {
    // 当前未实现，可以添加属性修改的逻辑
}

bool ModifierBuffUtil::IsMovementSpeedBuff(const BuffTable* buffTable) {
    return buffTable->bufftype() == kBuffTypeMovementSpeedReduction || 
           buffTable->bufftype() == kBuffTypeMovementSpeedBoost;
}
