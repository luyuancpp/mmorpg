#include "actor_attribute_calculator_system.h"

#include "game_logic/actor/comp/actor_atrribute_comp.h"
#include "game_logic/actor/util/actor_attribute_calculator_util.h"
#include "thread_local/storage.h"

extern  std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kAttributeConfigs;

void ActorAttributeCalculatorSystem::Update(double delta)
{
    for (auto&& [entity, actorAttributeBitSetComp] : tls.registry.view<ActorAttributeBitSetComp>().each())
    {
        auto& attributeBits = actorAttributeBitSetComp.attributeBits;
        for (const auto& [attributeIndex, updateFunction] : kAttributeConfigs) {
            if (updateFunction && attributeBits.test(attributeIndex)) {
                updateFunction(entity);
                attributeBits.reset(attributeIndex);
            }
        }
    }
}