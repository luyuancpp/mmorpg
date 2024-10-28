#include "actor_attribute_calculator_util.h"
#include <array>
#include <bitset>
#include "proto/logic/component/actor_status_comp.pb.h"
#include "game_logic/actor/comp/actor_atrribute_comp.h"
#include "game_logic/actor/constants/actor_state_attribute_calculator_constants.h"
#include "thread_local/storage.h"

// 初始化计算工具
void ActorAttributeCalculatorUtil::Initialize() {}

// 初始化属性组件
void ActorAttributeCalculatorUtil::InitializeActorComponents(entt::entity entity) {
    tls.registry.emplace<ActorAttributeBitSetComp>(entity);
}

// 更新速度属性
void UpdateVelocity(entt::entity entity) {
    // 计算速度属性逻辑
    // 示例：calculatedAttributes.currentVelocity = BaseAttributes.velocity + derivedAttributes.velocityBonus;
}

// 更新生命值属性
void UpdateHealth(entt::entity entity) {
    // 计算生命值属性逻辑
}

// 更新能量值属性
void UpdateEnergy(entt::entity entity) {
    // 计算能量值属性逻辑
}

// 更新状态效果
void UpdateStatusEffects(entt::entity entity) {
    // 更新实体的状态效果（例如中毒、减速等）
}

// 属性计算配置
struct AttributeCalculatorConfig {
    uint32_t attributeIndex;  // 属性索引
    void (*updateFunction)(entt::entity);  // 属性更新函数指针
};

// 定义一个属性和对应计算函数的映射表
constexpr std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kAttributeConfigs = {{
    {kVelocity, UpdateVelocity},
    // 可以在这里继续添加其他属性的计算函数
}};

// 更新实体状态属性
void ActorAttributeCalculatorUtil::UpdateActorState(entt::entity actorEntity) {
    auto& attributeBits = tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits;

    // 遍历每个属性，如果对应的位被设置，则执行属性计算函数
    for (const auto& config : kAttributeConfigs) {
        if (attributeBits.test(config.attributeIndex)) {
            config.updateFunction(actorEntity);  // 调用对应的计算函数
            attributeBits.reset(config.attributeIndex);  // 重置位，表示属性已经重新计算
        }
    }
}

// 设置某个属性需要更新
void ActorAttributeCalculatorUtil::MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit) {
    auto& attributeBits = tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits;
    attributeBits.set(attributeBit);  // 标记对应的位，表示该属性需要更新
}
