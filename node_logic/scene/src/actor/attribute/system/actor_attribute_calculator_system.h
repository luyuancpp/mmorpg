#pragma once
#pragma once
#include <functional>
#include <entt/src/entt/entity/entity.hpp>

// 属性计算配置
struct AttributeCalculatorConfig {
    uint32_t attributeIndex{0};  // 属性索引
    std::function<void(entt::entity)> updateFunction;;  // 属性更新函数指针
};

class ActorAttributeCalculatorSystem {
public:
    static void Initialize();

    static void Update(double delta);

    static void InitializeActorComponents(entt::entity entity);

    static void MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit);

    static void ImmediateCalculateAttributes(entt::entity entity, uint32_t attributeBit);
};

