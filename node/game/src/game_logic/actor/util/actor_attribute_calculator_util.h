#pragma once
#include <entt/src/entt/entity/entity.hpp>

// 属性计算配置
struct AttributeCalculatorConfig {
    uint32_t attributeIndex;  // 属性索引
    void (*updateFunction)(entt::entity);  // 属性更新函数指针
};

class ActorAttributeCalculatorUtil {
public:
    static void Initialize();

    static void InitializeActorComponents(entt::entity entity);

    static void MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit);
};
