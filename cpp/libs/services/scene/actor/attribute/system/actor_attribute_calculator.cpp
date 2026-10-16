#include "actor_attribute_calculator.h"

#include <algorithm>
#include <array>
#include <ranges>

#include "actor/attribute/comp/actor_attribute_comp.h"
#include "macros/return_define.h"
#include "proto/common/component/actor_attribute_state_comp.pb.h"

#include "table/code/buff_table.h"
#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "combat/buff/comp/buff_comp.h"
#include "player/comp/player_frozen_comp.h"  // cross-zone-readiness-audit.md §11.2
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/common/component/actor_combat_state_comp.pb.h"
#include <generated/attribute/actorbaseattributess2c_attribute_sync.h>
#include <thread_context/ecs_context.h>  // tlsEcs + entt::exclude (see movement.cpp pattern)

namespace {
// 初始化和后续重算共用同一投影规则;是否需要同步由各自调用入口决定。
void ProjectCombatStateFlags(CombatStateFlagsComp& projection, const CombatStateCollectionComp* combatStates)
{
    auto* stateFlags = projection.mutable_state_flags();
    stateFlags->clear();
    if (combatStates == nullptr) return;
    for (const auto& stateKey : combatStates->states() | std::views::keys)
    {
        stateFlags->emplace(stateKey, true);
    }
}
} // namespace

// 重算移速**属性**(标量)。
//
// 旧实现把 buff 的加减速标量灌进 Velocity 的 x/y/z 三轴 —— 而 MovementSystem
// 把 Velocity 当运动学矢量每 tick 积分进 Transform,于是挂移速 buff 的角色
// 沿 (1,1,1) 方向匀速漂移(减速则为负、钻入地下),几秒内被 AOI 划出所有
// 观察者视野,漂移后的位置还会随存盘落库。属性写进独立的 MoveSpeedComp,
// Velocity 留给真实的运动矢量(由未来的移动输入/AI 写入)。
// 移速属性目前无 S2C 通道(ActorBaseAttributesS2C.velocity 语义是运动矢量,
// 不能复用),接通道时再补脏位。
//
// MoveSpeedComp 是 actor 必需组件,唯一创建点是 InitializeOnCreate 的直接调用。
// 这里只取不建:缺了说明 actor 没走初始化(或组件被误删),补建一个 0 移速的组件只会把这个缺口藏起来。
void UpdateMoveSpeed(entt::entity entity) {
    auto* moveSpeedComp = tlsEcs.actorRegistry.try_get<MoveSpeedComp>(entity);
    if (moveSpeedComp == nullptr) {
        LOG_ERROR << "[ActorAttribute] MoveSpeedComp missing (actor not initialized?); move speed recalculation skipped: entity="
                  << entt::to_integral(entity);
        return;
    }

    // BuffListComp 是按需状态(契约见 buff.h:AddOrUpdateBuff 第一次插入条目时创建,BuffSystem::Update
    // 在帧末发现列表空了就摘掉),没有它就是身上没有 buff,移速加成按 0 算,不是缺组件故障。
    // 最后一条移速 buff 到期时 OnBuffRemove 标脏、同一帧列表被摘;以前这里直接 return,
    // 到期前的那份加成就会一直留在 MoveSpeedComp 里。
    double moveSpeed = 0.0;
    if (const auto* buffList = tlsEcs.actorRegistry.try_get<BuffListComp>(entity)) {
        for (const auto &buffCompPb : *buffList | std::views::values)
        {
            LookupBuffOrContinue(buffCompPb.buffPb.buff_table_id());

            moveSpeed += buffRow->movement_speed_boost();
            moveSpeed -= buffRow->movement_speed_reduction();
        }
    }

    // 减速叠满也不能变成倒着走。
    moveSpeedComp->moveSpeed = std::max(moveSpeed, 0.0);
}

void UpdateHealth(entt::entity actorEntity) {
    // TODO: Implement health recalculation from base stats + buff modifiers
}

void UpdateEnergy(entt::entity actorEntity) {
    // TODO: Implement energy recalculation from base stats + buff modifiers
}

// 把 CombatStateCollectionComp 的当前状态投影成客户端同步用的
// CombatStateFlagsComp,并置同步脏位。
//
// 旧实现三层皆错,任何一层都足以让眩晕/沉默永远同步不到客户端:
//   1) 写进 get_or_emplace<ActorBaseAttributesS2C>(实体上的一个死组件,
//      全仓零读者)—— 而生成序列化器读的是 try_get<CombatStateFlagsComp>;
//   2) 从不置 kCombatStateFlagsFieldNumber 脏位,序列化分支根本不执行;
//   3) 值写 false —— 而 CombatStateCollectionComp 键存在即代表状态激活
//      (RemoveCombatState 在 sources 清空时删键),语义是反的。
//
// CombatStateFlagsComp 是 actor 必需组件(同步投影),唯一创建点是
// InitializeOnCreate 的直接调用;这里只取不建,缺了就报错跳过,
// 不置同步脏位(没有投影可发)。CombatStateCollectionComp 是按需组件,没有就是没有战斗状态。
void ResetCombatStateFlags(entt::entity actorEntity) {
    auto* stateFlagsComp = tlsEcs.actorRegistry.try_get<CombatStateFlagsComp>(actorEntity);
    if (stateFlagsComp == nullptr) {
        LOG_ERROR << "[ActorAttribute] CombatStateFlagsComp missing (actor not initialized?); combat state sync skipped: entity="
                  << entt::to_integral(actorEntity);
        return;
    }
    ProjectCombatStateFlags(*stateFlagsComp,
        tlsEcs.actorRegistry.try_get<CombatStateCollectionComp>(actorEntity));

    SetActorBaseAttributesS2CAttrDirtyBit(actorEntity, static_cast<std::size_t>(ActorBaseAttributesS2C::kCombatStateFlagsFieldNumber));
    // entity_id 是这条消息里唯一的归属标识(信封层不带主体 actor):
    // 不一并置位的话,观察者会收到一条不知道是谁的状态更新。
    // 序列化器从 Guid(uint64_t)组件取值,客户端在 ActorCreateS2C.guid
    // 建立过 guid → actor 的映射,能够归属。
    SetActorBaseAttributesS2CAttrDirtyBit(actorEntity, static_cast<std::size_t>(ActorBaseAttributesS2C::kEntityIdFieldNumber));
}

std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kAttributeConfigs = { {
    {kMoveSpeed, UpdateMoveSpeed},
    {kHealth, UpdateHealth},
    {kEnergy, UpdateEnergy},
    {kCombatState, ResetCombatStateFlags}
} };

bool ActorAttributeCalculatorSystem::InitializeOnCreate(entt::entity actorEntity)
{
    auto& registry = tlsEcs.actorRegistry;
    if (!registry.valid(actorEntity))
    {
        LOG_ERROR << "[ActorAttribute] Invalid actor; initialization refused: entity="
                  << entt::to_integral(actorEntity);
        return false;
    }

    // 必需派生组件只在这个显式创建入口建立;可选来源缺失代表无加成/无状态。
    // 这里只确保组件存在，移速统一由下方 UpdateMoveSpeed 重算。
    static_cast<void>(registry.get_or_emplace<MoveSpeedComp>(actorEntity));
    auto& stateFlags = registry.get_or_emplace<CombatStateFlagsComp>(actorEntity);
    UpdateMoveSpeed(actorEntity);
    ProjectCombatStateFlags(stateFlags, registry.try_get<CombatStateCollectionComp>(actorEntity));
    return true;
}

// AttributeDirtyFlagsComp 是按需状态:标脏(这里)时挂上,Update 消费后摘掉。
// 以前只加不删、消费时只清位,组件留在实体上,每 tick 的 view 实际遍历了所有标过一次脏的 actor。
void ActorAttributeCalculatorSystem::MarkAttributeForUpdate(const entt::entity actorEntity, const uint32_t attributeBit) {
    // std::bitset::set 越界会抛 out_of_range;对已销毁的实体 emplace 在 EnTT 里是未定义行为
    // (buff / 战斗状态事件可能晚于实体销毁到达),两者都在动组件之前拦下。
    if (attributeBit >= kAttributeCalculatorMax) {
        LOG_ERROR << "[ActorAttribute] Invalid attribute bit; mark ignored: entity=" << entt::to_integral(actorEntity)
                  << " attributeBit=" << attributeBit;
        return;
    }
    if (!tlsEcs.actorRegistry.valid(actorEntity)) {
        LOG_ERROR << "[ActorAttribute] Invalid actor entity; mark ignored: entity=" << entt::to_integral(actorEntity)
                  << " attributeBit=" << attributeBit;
        return;
    }
    // 同一 tick 内多次标脏要合并位,不能 emplace_or_replace(那会丢掉先标的位)。
    if (auto* dirtyFlags = tlsEcs.actorRegistry.try_get<AttributeDirtyFlagsComp>(actorEntity)) {
        dirtyFlags->attributeBits.set(attributeBit);
        return;
    }
    tlsEcs.actorRegistry.emplace<AttributeDirtyFlagsComp>(actorEntity).attributeBits.set(attributeBit);
}

void ActorAttributeCalculatorSystem::Update()
{
    // PlayerFrozenComp exclude: a frozen player's attributes were
    // marshalled into PlayerAllData when the ownership handoff froze them
    // (PlayerLifecycleSystem::StartTravelHandoff).
    // Recomputing them on the source side now is wasted work — and
    // worse, the values would diverge from what the destination zone
    // reconstructs from the snapshot. Skip frozen entities until the
    // handoff resolves: granted → the entity goes away with
    // DestroyDeposedPlayer; not granted → AbortTravelHandoff removes the
    // marker. There is no ACK and no reaper any more.
    // cross-zone-readiness-audit.md §11.2 (only §11 of that doc is current).
    //
    // 消费即摘除:先把脏位拷出来再 remove 本实体的标记组件,view 里就只剩真正待重算的实体。
    // EnTT 允许在遍历中移除「当前实体」的被遍历组件(之后 dirtyFlags 引用失效,不能再碰);
    // 重算函数若又给本实体标脏,挂上的是一份新标记,留到下一 tick 处理,不会被这里吞掉。
    for (auto&& [entity, dirtyFlags] : tlsEcs.actorRegistry.view<AttributeDirtyFlagsComp>(
            entt::exclude<PlayerFrozenComp>).each())
    {
        const auto attributeBits = dirtyFlags.attributeBits;
        tlsEcs.actorRegistry.remove<AttributeDirtyFlagsComp>(entity);
        for (const auto& [attributeIndex, updateFunction] : kAttributeConfigs) {
            if (updateFunction && attributeBits.test(attributeIndex)) {
                updateFunction(entity);
            }
        }
    }
}
