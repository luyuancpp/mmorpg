#include "modifier_buff_system.h"

#include "buff_config.h"
#include "buff_config.pb.h"
#include "component/actor_attribute_state_comp.pb.h"
#include "game_logic/actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "game_logic/actor/attribute/system/actor_attribute_calculator_system.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"

bool ModifierBuffSystem::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorSystem::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

void ModifierBuffSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp) {
    // 当前未实现，可以添加逻辑来刷新 buff
}

bool ModifierBuffSystem::OnBuffRemove(const entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorSystem::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

bool ModifierBuffSystem::OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    // 当前未实现，可以添加销毁 buff 的逻辑

    if (buffTable == nullptr) {
        return false;
    }

    
    return false;
}

static bool OnHealthRegenerationBasedOnLostHealth(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }

    //todo 及时计算 max_health
    auto& baseAttributesPbComponent = tls.registry.get<BaseAttributesPbComponent>(parent);
    const auto& derivedAttributesPbComponent = tls.registry.get<DerivedAttributesPbComponent>(parent);
    const auto& levelComponent = tls.registry.get<LevelPbComponent>(parent);

    const auto lostHealth = derivedAttributesPbComponent.max_health() - baseAttributesPbComponent.health();  // 计算已损失生命值

    BuffConfigurationTable::Instance().SetHealthregenerationParam(
        { static_cast<double>(levelComponent.level()),  static_cast<double>(lostHealth) });

    const auto healingAmount = BuffConfigurationTable::Instance().GetHealthregeneration(buffTable->id());
    const auto currentHealth = std::min<uint64_t>(derivedAttributesPbComponent.max_health(),
        static_cast<uint64_t>(static_cast<double>(baseAttributesPbComponent.health()) +
            healingAmount));

    baseAttributesPbComponent.set_health(currentHealth);

    LOG_TRACE << "Healing applied, current health: " << currentHealth;

    return  true;
}

bool ModifierBuffSystem::OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{

    switch (buffTable->bufftype())
    {
    case kBuffTypeHealthRegenerationBasedOnLostHealth:
    {
        OnHealthRegenerationBasedOnLostHealth(parent, buffComp, buffTable);
        return true;
    }
    break;
    default:
        return false;
        break;
    }
    return false;
}

void ModifierBuffSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
}

bool ModifierBuffSystem::IsMovementSpeedBuff(const BuffTable* buffTable) {
    return buffTable->bufftype() == kBuffTypeMovementSpeedReduction || 
           buffTable->bufftype() == kBuffTypeMovementSpeedBoost;
}
