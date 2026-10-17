#pragma once

#include <functional>
#include <entt/src/entt/entity/entity.hpp>

struct AttributeCalculatorConfig {
    uint32_t attributeIndex{0};
    std::function<void(entt::entity)> updateFunction;
};

// actor 属性的延迟重算:写方标脏,每 tick 由 Update 统一消费。
// 线程模型:只在 scene 节点 loop 线程调用。
class ActorAttributeCalculatorSystem {
public:
    // actor 创建流程在 Unmarshal 之后对新实体直接调用一次,挂上空的同步投影 CombatStateFlagsComp。
    // 调用方保证 actorEntity 是刚创建的有效实体,不能重复调用。
    static void InitializeOnCreate(entt::entity actorEntity);

    // 给 actor 标记一项待重算属性(attributeBit 取 eAttributeCalculator)。
    // 脏标记组件按需挂上、Update 消费后摘掉;同一 tick 内多次标脏合并。
    // 实体无效或位越界时只记错误、不改任何组件。
    static void MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit);

    // 消费全部脏标记(跨 zone 冻结中的玩家除外,留到冻结解除)。
    // 重算函数只写 actor 初始化时挂好的必需组件,缺组件的实体记错误后跳过,不补建。
    static void Update();

};

