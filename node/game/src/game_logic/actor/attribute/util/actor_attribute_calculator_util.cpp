// actor_attribute_calculator_util.cpp
#include "actor_attribute_calculator_util.h"
#include <array>

#include "buff_config.h"
#include "game_logic/actor/attribute/comp/actor_atrribute_comp.h"
#include "game_logic/actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "proto/logic/client_player/player_state_attribute_sync.pb.h"
#include "proto/logic/component/actor_combat_state_comp.pb.h"
#include "thread_local/storage.h"

// 初始化属性计算工具，不执行任何操作，但为将来可能的初始化逻辑预留
void ActorAttributeCalculatorUtil::Initialize() {}

// 初始化给定实体的属性组件
void ActorAttributeCalculatorUtil::InitializeActorComponents(entt::entity entity) {
    tls.registry.emplace<ActorAttributeBitSetComp>(entity);
}

// 更新速度属性
void UpdateVelocity(entt::entity entity) {
    auto& velocity =  tls.registry.get<Velocity>(entity);
    velocity.Clear();
    
    for (const auto&  buffCompPb : tls.registry.get<BuffListComp>(entity) | std::views::values){
        FetchBuffTableOrContinue(buffCompPb.buffPb.buff_table_id());

        velocity.set_x(velocity.x() + buffTable->movement_speed_boost());
        velocity.set_y(velocity.y() + buffTable->movement_speed_boost());
        velocity.set_z(velocity.z() + buffTable->movement_speed_boost());

        velocity.set_x(velocity.x() - buffTable->movement_speed_reduction());
        velocity.set_y(velocity.y() - buffTable->movement_speed_reduction());
        velocity.set_z(velocity.z() - buffTable->movement_speed_reduction());
    }

    tls.registry.get<BaseAttributeDeltaS2C>(entity).mutable_velocity()->CopyFrom(velocity);
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

void UpdateCombatState(entt::entity actorEntityId) {
    auto& combatStateCollectionComponent = tls.registry.get<CombatStateCollectionPbComponent>(actorEntityId);
    
    auto& baseAttributeDeltaS2C = tls.registry.get<BaseAttributeDeltaS2C>(actorEntityId);

    for (const auto& key : combatStateCollectionComponent.state_list() | std::views::keys)
    {
        baseAttributeDeltaS2C.mutable_combat_state_flags()->mutable_state_flags()->emplace(key, false);
    }
}

// 定义属性与计算函数的映射表
std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kAttributeConfigs = {{
    {kVelocity, UpdateVelocity},
    {kHealth, UpdateHealth},
    {kEnergy, UpdateEnergy},
    {kCombatState, UpdateCombatState}
    //{kStatusEffects, UpdateStatusEffects}
    // 可以继续添加其他属性和计算函数
}};

// 标记属性需要更新的位
void ActorAttributeCalculatorUtil::MarkAttributeForUpdate(const entt::entity actorEntity, const uint32_t attributeBit) {
    auto& attributeBits = tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits;
    attributeBits.set(attributeBit);  // 设置指定位，表示该属性需要更新
}

void ActorAttributeCalculatorUtil::ImmediateCalculateAttributes(const entt::entity actorEntity, const uint32_t attributeBit)
{
    if (attributeBit >= kAttributeConfigs.size()){
        return;
    }

    if (const auto& attributeBits = tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits;
        !attributeBits.test(attributeBit)){
        return;
    } 
    
    const auto& updateFunction = kAttributeConfigs[attributeBit].updateFunction;
    if (!updateFunction){
        return;
    }
    
    updateFunction(actorEntity);
}
