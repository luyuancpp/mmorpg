#pragma once
#include <entt/src/entt/entity/entity.hpp>

#include "buff_impl_util.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "proto/logic/component/actor_status_comp.pb.h"

class ModifierBuffUtilImplUtil{
public:
    static bool OnIntervalThink(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
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
    
    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity)
    {
    }

    static bool OnHealthRegenerationBasedOnLostHealth(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
    {
        if (buffTable == nullptr ) {
            return false;
        }

        //todo 及时计算 max_health
        auto& baseAttributesPbComponent = tls.registry.get<BaseAttributesPbComponent>(parent);
        const auto& derivedAttributesPbComponent = tls.registry.get<DerivedAttributesPbComponent>(parent);
        const auto& levelComponent = tls.registry.get<LevelPbComponent>(parent);

        const auto lostHealth = derivedAttributesPbComponent.max_health() - baseAttributesPbComponent.health();  // 计算已损失生命值

        BuffConfigurationTable::Instance().SetHealthregenerationParam(
            { static_cast<double>(levelComponent.level()),  static_cast<double>(lostHealth)});

        const auto healingAmount = BuffConfigurationTable::Instance().GetHealthregeneration(buffTable->id());
        const auto currentHealth = std::min<uint64_t>(derivedAttributesPbComponent.max_health(),
                                            static_cast<uint64_t>(static_cast<double>(baseAttributesPbComponent.health()) +
                                                healingAmount));

        baseAttributesPbComponent.set_health(currentHealth);
        
        LOG_TRACE << "Healing applied, current health: " << currentHealth ;
        
        return  true;
    }
};
