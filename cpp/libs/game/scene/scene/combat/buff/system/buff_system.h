#pragma once

#include "scene/combat/buff/comp/buff_comp.h"
#include "base/common/type_define/type_define.h"

class BuffTable;
class SkillExecutedEvent;

//https://zhuanlan.zhihu.com/p/150812545

//Caster（Buff施加者），Parent（Buff当前挂载的目标）,
//Skill(Buff由哪个技能创建)，BuffLayer（层数）,
//BuffLevel（等级）BuffDuration（时长），
//BuffTag，BuffImmuneTag（免疫BuffTag）
//以及Context(Buff创建时的一些相关上下文数据)等等。

class DamageEventPbComponent;
struct BuffComp;

class BuffSystem
{
public:
    static void Update(double delta);

    static void InitializeActorComponents(entt::entity entity);

    static std::tuple<uint32_t, uint64_t>  AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext);

    static std::tuple<uint32_t, uint64_t> AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId);

    static void RemoveBuff(entt::entity parent, uint64_t buffId);

    static void RemoveBuff(entt::entity parent, const UInt64Set& removeBuffIdList);

    static void RemoveSubBuff(BuffComp& buffComp, UInt64Set& buffsToRemove);

    static void MarkBuffForRemoval(entt::entity parent, uint64_t buffId);

    // 帧结束时统一移除所有待移除的 Buff
    static void RemovePendingBuffs(entt::entity parent, BuffListComp& buffListComp);

    //Buff创建前检查当前Buff是否可创建。
    //一般主要是检测目标身上是否存在免疫该Buff的相关Buff，如果被免疫则不会创建该Buff。
    static uint32_t CanCreateBuff(entt::entity parentEntity, uint32_t buffTableId);

    static bool HandleExistingBuff(entt::entity parentEntity, uint32_t buffTableId, const SkillContextPtrComp& abilityContext);

    //Buff在实例化之后，生效之前（还未加入到Buff容器中）时会抛出一个OnBuffAwake事件。
    // 如果存在某种Buff的效果是：受到负面效果时，驱散当前所有负面效果，并给自己加一个护盾。
    // 那么这个时候就需要监听BuffAwake事件了，此时会给自己加护盾，
    // 并且把所有负面Buff驱散。这意味着一个Buff可能还未生效之前即销毁了（小心Buff的生命周期）。
    static uint32_t OnBuffAwake(entt::entity parent, uint32_t buffTableId);

    //当Buff生效时（加入到Buff容器后），我们提供给策划一个抽象接口OnBuffStart，由策划配置具体效果。
    static void OnBuffStart(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable);

    //当Buff添加时存在相同类型且Caster相等的时候，Buff执行刷新流程（更新Buff层数，等级，持续时间等数据）。
    // 我们提供给策划一个抽象接口OnBuffRefresh，由策划配置具体效果。
    static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp);

    //当Buff销毁前（还未从Buff容器中移除），我们提供给策划一个抽象接口OnBuffRemove，由策划配置具体效果。
    static void OnBuffRemove(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable);

    //当Buff销毁后（已从Buff容器中移除），我们提供给策划一个抽象接口OnBuffDestroy，由策划配置具体效果。
    static void OnBuffDestroy(entt::entity parent, const uint64_t buffId, const BuffTable* buffTable);

    //Buff还可以创建定时器，以触发间隔持续效果。通过策划配置时调用StartIntervalThink操作，
    // 提供OnIntervalThink抽象接口供策划配置具体效果。

    static void OnIntervalThink(entt::entity parent, uint64_t buffId);

    static void OnBuffExpire(entt::entity parent, uint64_t buffId);

    //监听某个主动技能执行成功
    static void OnSkillExecuted(SkillExecutedEvent& event);

    //监听我方给目标造成伤害时触发
    static void OnBeforeGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent);

    //监听我方给目标造成伤害时触发
    static void OnAfterGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent);

    //监听我方受到伤害时触发
    static void OnBeforeTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent);

    //监听我方受到伤害时触发
    static void OnAfterTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent);

    //监听我方死亡时触发
    static void OnBeforeDead(entt::entity parent);

    //监听我方死亡时触发
    static void OnAfterDead(entt::entity parent);

    //监听我方击杀目标时触发
    static void OnKill(entt::entity parent);

    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity);

    static bool AddSubBuffs(
        entt::entity parent,
        const BuffTable* buffTable,
        BuffComp& buffComp  // 根据实际类型填写
    );

    static void AddTargetSubBuffs(
        entt::entity targetEntity,
        const BuffTable* buffTable,
        const SkillContextPtrComp& abilityContext
    );

    static void AddSubBuffsWithoutCheck(
        entt::entity parent,
        const BuffTable* buffTable,
        BuffComp& buffComp  // 根据实际类型填写
    );
};


