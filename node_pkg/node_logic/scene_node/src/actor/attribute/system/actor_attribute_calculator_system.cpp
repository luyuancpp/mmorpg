#include "actor_attribute_calculator_system.h"

#include <array>
#include <ranges>

#include "actor/attribute/comp/actor_atrribute_comp.h"
#include "thread_local/storage.h"
#include "buff_config.h"
#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "combat/buff/comp/buff_comp.h"
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/logic/component/actor_combat_state_comp.pb.h"

// 初始化属性计算工具，不执行任何操作，但为将来可能的初始化逻辑预留
void ActorAttributeCalculatorSystem::Initialize() {}

// 初始化给定实体的属性组件
void ActorAttributeCalculatorSystem::InitializeActorComponents(entt::entity entity) {
    tls.registry.emplace<ActorAttributeBitSetComp>(entity);
}

// 更新速度属性
void UpdateVelocity(entt::entity entity) {
    return;
    auto& velocity = tls.registry.get<Velocity>(entity);
    velocity.Clear();

    for (const auto& buffCompPb : tls.registry.get<BuffListComp>(entity) | std::views::values) {
        FetchBuffTableOrContinue(buffCompPb.buffPb.buff_table_id());

        velocity.set_x(velocity.x() + buffTable->movement_speed_boost());
        velocity.set_y(velocity.y() + buffTable->movement_speed_boost());
        velocity.set_z(velocity.z() + buffTable->movement_speed_boost());

        velocity.set_x(velocity.x() - buffTable->movement_speed_reduction());
        velocity.set_y(velocity.y() - buffTable->movement_speed_reduction());
        velocity.set_z(velocity.z() - buffTable->movement_speed_reduction());
    }

    tls.registry.get<BaseAttributeSyncDataS2C>(entity).mutable_velocity()->CopyFrom(velocity);
}

// 更新生命值属性
void UpdateHealth(entt::entity actorEntityId) {
    // 计算生命值属性的逻辑
}

// 更新能量值属性
void UpdateEnergy(entt::entity actorEntityId) {
    // 计算能量值属性的逻辑
}

// 更新状态效果属性
void UpdateStatusEffects(entt::entity actorEntityId) {
    // 更新实体的状态效果，例如中毒、减速等
}

void ResetCombatStateFlags(entt::entity actorEntity) {
    // 获取 combatStateCollection 组件（包含所有状态）
    const auto& combatStates = tls.registry.get<CombatStateCollectionPbComponent>(actorEntity);

    // 获取基础属性的同步数据
    auto& syncData = tls.registry.get<BaseAttributeSyncDataS2C>(actorEntity);

    // 获取指向状态标志的指针
    auto* stateFlags = syncData.mutable_combat_state_flags()->mutable_state_flags();

    // 清空现有状态标志
    stateFlags->clear();

    // 遍历所有状态键并初始化为 false
    for (const auto& stateKey : combatStates.states() | std::views::keys) {
        stateFlags->emplace(stateKey, false);
    }
}

// 定义属性与计算函数的映射表
std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kAttributeConfigs = { {
    {kVelocity, UpdateVelocity},
    {kHealth, UpdateHealth},
    {kEnergy, UpdateEnergy},
    {kCombatState, ResetCombatStateFlags}
    //{kStatusEffects, UpdateStatusEffects}
    // 可以继续添加其他属性和计算函数
} };

// 标记属性需要更新的位
void ActorAttributeCalculatorSystem::MarkAttributeForUpdate(const entt::entity actorEntity, const uint32_t attributeBit) {
    auto& attributeBits = tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits;
    attributeBits.set(attributeBit);  // 设置指定位，表示该属性需要更新
}

void ActorAttributeCalculatorSystem::ImmediateCalculateAttributes(const entt::entity actorEntity, const uint32_t attributeBit)
{
    if (attributeBit >= kAttributeConfigs.size()) {
        return;
    }

    if (const auto& attributeBits = tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits;
        !attributeBits.test(attributeBit)) {
        return;
    }

    const auto& updateFunction = kAttributeConfigs[attributeBit].updateFunction;
    if (!updateFunction) {
        return;
    }

    updateFunction(actorEntity);
}


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