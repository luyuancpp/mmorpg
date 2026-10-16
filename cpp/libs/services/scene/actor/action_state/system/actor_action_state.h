#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

// actor 行为状态(战斗中 / 跟随 / 骑乘)的互斥与打断。
// 前置条件:actorEntity 是 actorRegistry 中的活实体;失效实体在任何状态副作用之前拒绝。
// ActorStateComp 按需存在,缺失表示没有激活状态。AddState 是唯一延迟创建入口;
// 查询不创建,RemoveState 对不存在的状态幂等成功,HasState 对缺失或失效实体答 false。
// 线程模型:只在 scene 节点 loop 线程调用。
class ActorActionStateSystem
{
public:
    static uint32_t TryPerformAction(entt::entity actorEntity, uint32_t actorAction, uint32_t successState);

    static uint32_t CanExecuteActionWithoutStateChange(entt::entity actorEntity, uint32_t actorAction);
    
    static bool HasState(entt::entity actorEntity, uint32_t state);

    static uint32_t AddState(entt::entity actorEntity, uint32_t actorState);

    static uint32_t RemoveState(entt::entity actorEntity, uint32_t actorState);

};
