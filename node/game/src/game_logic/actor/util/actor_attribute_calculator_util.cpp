#include "actor_attribute_calculator_util.h"
#include "component/actor_status_comp.pb.h"
#include "game_logic/actor/comp/actor_atrribute_comp.h"
#include "game_logic/actor/constants/actor_state_attribute_calculator_constants.h"
#include "thread_local/storage.h"



void ActorAttributeCalculatorUtil::Initialize() {
}

void ActorAttributeCalculatorUtil::InitializeActorComponents(entt::entity entity)
{
    tls.registry.emplace<ActorAttributeBitSetComp>(entity);
}


// 更新生命值
void UpdateVelocity(const entt::entity enttiy) {
    // 计算当前生命值，示例为基础生命值 + 血量增益 - 伤害值
    //calculatedAttributes.currentHealth = BaseAttributesPBComponent.baseHealth + derivedAttributes.healthBonus - derivedAttributes.damageTaken;

    // 确保生命值不低于零
    //calculatedAttributes.currentHealth = std::max(calculatedAttributes.currentHealth, 0.0);
}

// 更新生命值
void UpdateHealth(entt::entity enttiy) {
    // 计算当前生命值，示例为基础生命值 + 血量增益 - 伤害值
    //calculatedAttributes.currentHealth = BaseAttributesPBComponent.baseHealth + derivedAttributes.healthBonus - derivedAttributes.damageTaken;

    // 确保生命值不低于零
    //calculatedAttributes.currentHealth = std::max(calculatedAttributes.currentHealth, 0.0);
}

// 更新能量值
static void UpdateEnergy(entt::entity enttiy) {
    // 计算当前能量值，示例为基础能量值 + 能量增益 - 能量消耗
    //calculatedAttributes.currentEnergy = BaseAttributesPBComponent.baseEnergy + derivedAttributes.energyBonus - derivedAttributes.energyUsed;

    // 确保能量值不低于零
    //calculatedAttributes.currentEnergy = std::max(calculatedAttributes.currentEnergy, 0.0);
}

// 更新状态效果
void UpdateStatusEffects(entt::entity enttiy) {
    // 示例逻辑：检查并更新状态效果（如中毒、减速等）
    //auto& statusEffects = tls.registry.get<StatusEffectsComponent>(actorEntity);

    //for (const auto& effect : statusEffects.activeEffects) {
        // 根据状态效果更新角色的属性
       // ApplyStatusEffect(effect, derivedAttributes);
    //}
}

// 应用单个状态效果
static void ApplyStatusEffect(entt::entity enttiy) {
    // 处理不同类型的状态效果
    //switch (effect.type) {
    //case StatusEffectType::Poison:
    //    derivedAttributes.damageTaken += effect.value; // 增加伤害
    //    break;
    //case StatusEffectType::Slow:
    //    derivedAttributes.movementSpeed -= effect.value; // 降低移动速度
    //    break;
    //    // 其他状态效果...
    //default:
    //    break;
    //}
}

// 计算并更新角色的状态属性
void ActorAttributeCalculatorUtil::UpdateActorState(entt::entity actorEntity) {
    //// 获取角色的基础属性
    //const auto& BaseAttributesPBComponent = tls.registry.get<BaseAttributesPBComponent>(actorEntity);
    //auto& derivedAttributes = tls.registry.get<DerivedAttributesPBComponent>(actorEntity);
    //auto& calculatedAttributes = tls.registry.get<CalculatedAttributesPBComponent>(actorEntity);

    //// 计算生命值
    //UpdateHealth(BaseAttributesPBComponent, derivedAttributes, calculatedAttributes);

    //// 计算能量值
    //UpdateEnergy(BaseAttributesPBComponent, derivedAttributes, calculatedAttributes);

    //// 计算其他状态属性
    //UpdateStatusEffects(actorEntity, derivedAttributes);
}

struct AttributeCalculatorConfig
{
    uint32_t attributeIndex = 0;
    void (*updateFunction)(const entt::entity);

};

constexpr std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kDistanceSyncConfigs = {
    {eAttributeCalculator::kVelocity, UpdateVelocity},
};

void ActorAttributeCalculatorUtil::Update(entt::entity actorEntity, uint32_t attributeBit)
{
    tls.registry.get<ActorAttributeBitSetComp>(actorEntity).attributeBits.set(attributeBit);
}
