#include "modifier_buff_impl.h"

#include "table/code/buff_table.h"
#include "table/proto/buff_table.pb.h"
#include "proto/common/component/actor_attribute_state_comp.pb.h"
#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "actor/attribute/system/actor_attribute_calculator.h"
#include "combat/buff/comp/buff.h"
#include "combat/buff/constants/buff.h"

#include <thread_context/registry_manager.h>

bool ModifierBuffImplSystem::OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorSystem::MarkAttributeForUpdate(parent, kVelocity);
        return true;
    }

    return false;
}

void ModifierBuffImplSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffEntry& buffComp) {
}

bool ModifierBuffImplSystem::OnBuffRemove(const entt::entity parent, BuffEntry& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorSystem::MarkAttributeForUpdate(parent, kVelocity);
        return true;
    }

    return false;
}

bool ModifierBuffImplSystem::OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable) {
    return false;
}

static bool OnHealthRegenerationBasedOnLostHealth(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }

    // TODO: Compute max_health on demand
    auto& baseAttributesPbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<BaseAttributesComp>(parent);
    const auto& derivedAttributesPbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<DerivedAttributesComp>(parent);
    const auto& levelComponent = tlsRegistryManager.actorRegistry.get_or_emplace<LevelComp>(parent);

    const auto lostHealth = derivedAttributesPbComponent.max_health() - baseAttributesPbComponent.health();

    BuffTableManager::Instance().SetHealthregenerationParam(
        { static_cast<double>(levelComponent.level()),  static_cast<double>(lostHealth) });

    const auto healingAmount = BuffTableManager::Instance().GetHealthregeneration(buffTable->id());
    const auto currentHealth = std::min<uint64_t>(derivedAttributesPbComponent.max_health(),
        static_cast<uint64_t>(static_cast<double>(baseAttributesPbComponent.health()) +
            healingAmount));

    baseAttributesPbComponent.set_health(currentHealth);

    LOG_TRACE << "Healing applied, current health: " << currentHealth;

    return true;
}

bool ModifierBuffImplSystem::OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable)
{
    switch (buffTable->bufftype())
    {
    case kBuffTypeHealthRegenerationBasedOnLostHealth:
        return OnHealthRegenerationBasedOnLostHealth(parent, buffComp, buffTable);
    default:
        return false;
    }
}

void ModifierBuffImplSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
}

bool ModifierBuffImplSystem::IsMovementSpeedBuff(const BuffTable* buffTable) {
    return buffTable->bufftype() == kBuffTypeMovementSpeedReduction || 
           buffTable->bufftype() == kBuffTypeMovementSpeedBoost;
}

