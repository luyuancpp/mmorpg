﻿#pragma once

#include <entt/src/entt/entity/entity.hpp>
#include "game_logic/combat/buff/comp/buff_comp.h"

class buff_row;
class BuffTable;
class AbilityExecutedEvent;

//https://zhuanlan.zhihu.com/p/150812545

//Caster（Buff施加者），Parent（Buff当前挂载的目标）,
//Ability(Buff由哪个技能创建)，BuffLayer（层数）,
//BuffLevel（等级）BuffDuration（时长），
//BuffTag，BuffImmuneTag（免疫BuffTag）
//以及Context(Buff创建时的一些相关上下文数据)等等。

class BuffUtil
{
public:

    static void Initialize();

    static uint32_t AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext);

    static void OnBuffExpire(entt::entity parent, uint64_t buffId);

    //Buff创建前检查当前Buff是否可创建。
    //一般主要是检测目标身上是否存在免疫该Buff的相关Buff，如果被免疫则不会创建该Buff。
    static uint32_t CanCreateBuff(entt::entity parent, uint32_t buffTableId);

    static bool HandleExistingBuff(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext);

    //Buff在实例化之后，生效之前（还未加入到Buff容器中）时会抛出一个OnBuffAwake事件。
    // 如果存在某种Buff的效果是：受到负面效果时，驱散当前所有负面效果，并给自己加一个护盾。
    // 那么这个时候就需要监听BuffAwake事件了，此时会给自己加护盾，
    // 并且把所有负面Buff驱散。这意味着一个Buff可能还未生效之前即销毁了（小心Buff的生命周期）。
    static bool OnBuffAwake(entt::entity parent, uint32_t buffTableId);

    //当Buff生效时（加入到Buff容器后），我们提供给策划一个抽象接口OnBuffStart，由策划配置具体效果。
    static void OnBuffStart(entt::entity parent, uint64_t buffId);

    //当Buff添加时存在相同类型且Caster相等的时候，Buff执行刷新流程（更新Buff层数，等级，持续时间等数据）。
    // 我们提供给策划一个抽象接口OnBuffRefresh，由策划配置具体效果。
    static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext, BuffComp& buffComp);

    //当Buff销毁前（还未从Buff容器中移除），我们提供给策划一个抽象接口OnBuffRemove，由策划配置具体效果。
    static void OnBuffRemove(entt::entity parent, uint64_t buffId);

    //当Buff销毁后（已从Buff容器中移除），我们提供给策划一个抽象接口OnBuffDestroy，由策划配置具体效果。
    static void OnBuffDestroy(entt::entity parent, uint32_t buffTableId);

    //Buff还可以创建定时器，以触发间隔持续效果。通过策划配置时调用StartIntervalThink操作，
    // 提供OnIntervalThink抽象接口供策划配置具体效果。
    static void StartIntervalThink(entt::entity parent, uint64_t buffId);

    static void OnIntervalThink(entt::entity parent, uint64_t buffId);
    
    //Buff还可以通过请求改变运动来触发相关效果。
    //通过策划配置时调用ApplyMotion操作，提供OnMotionUpdate和OnMotionInterrupt接口供策划配置具体效果。
    static void ApplyMotion();

    static void OnMotionUpdate();

    static void OnMotionInterrupt();

    //监听某个主动技能执行成功
    static void OnAbilityExecuted(AbilityExecutedEvent& event);

    //监听我方给目标造成伤害时触发
    static void OnBeforeGiveDamage(entt::entity parent);

    //监听我方给目标造成伤害时触发
    static void OnAfterGiveDamage(entt::entity parent);

    //监听我方受到伤害时触发
    static void OnBeforeTakeDamage(entt::entity parent);

    //监听我方受到伤害时触发
    static void OnAfterTakeDamage(entt::entity parent);   

    //监听我方死亡时触发
    static void OnBeforeDead(entt::entity parent);

    //监听我方死亡时触发
    static void OnAfterDead(entt::entity parent);

    //监听我方击杀目标时触发
    static void OnKill(entt::entity parent);

};