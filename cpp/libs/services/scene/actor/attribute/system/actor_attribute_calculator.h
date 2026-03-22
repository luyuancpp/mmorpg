#pragma once
#pragma once
#include <functional>
#include <entt/src/entt/entity/entity.hpp>

struct AttributeCalculatorConfig {
    uint32_t attributeIndex{0};
    std::function<void(entt::entity)> updateFunction;;
};

class ActorAttributeCalculatorSystem {
public:
    static void Initialize();
    static void MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit);
    static void ImmediateCalculateAttributes(entt::entity actorEntity, uint32_t attributeBit);
    static void Update(double delta);

};

