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
    // actor 创建流程在首个通知之前直接调用,不依赖 dispatcher 订阅。
    // 建立并立即计算移速/战斗状态投影;重复调用按当前来源重算,不清掉已有状态或脏位。
    // 只创建必需投影,不预挂冷却、Buff、技能上下文等按需状态,也不为初始化标同步脏位。
    // 实体无效时返回 false,不改变组件集合;调用方必须中止本次创建。
    [[nodiscard]] static bool InitializeOnCreate(entt::entity actorEntity);

    // 给 actor 标记一项待重算属性(attributeBit 取 eAttributeCalculator)。
    // 脏标记组件按需挂上、Update 消费后摘掉;同一 tick 内多次标脏合并。
    // 实体无效或位越界时只记错误、不改任何组件。
    static void MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit);

    // 消费全部脏标记(跨 zone 冻结中的玩家除外,留到冻结解除)。
    // 重算函数只写 actor 初始化时挂好的必需组件,缺组件的实体记错误后跳过,不补建。
    static void Update();

};

