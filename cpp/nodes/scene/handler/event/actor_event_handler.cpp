#include "actor_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
#include "combat/skill/comp/skill_comp.h"
#include "muduo/base/Logging.h"
#include "proto/common/component/time_comp.pb.h"
///<<< END WRITING YOUR CODE
void ActorEventHandler::Register()
{
    tlsEcs.dispatcher.sink<InitializeActorCompsEvent>().connect<&ActorEventHandler::InitializeActorCompsEventHandler>();
    tlsEcs.dispatcher.sink<InterruptCurrentStateEvent>().connect<&ActorEventHandler::InterruptCurrentStateEventHandler>();
}

void ActorEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<InitializeActorCompsEvent>().disconnect<&ActorEventHandler::InitializeActorCompsEventHandler>();
    tlsEcs.dispatcher.sink<InterruptCurrentStateEvent>().disconnect<&ActorEventHandler::InterruptCurrentStateEventHandler>();
}
void ActorEventHandler::InitializeActorCompsEventHandler(const InitializeActorCompsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    // 技能链路依赖的两张表在 actor 创建时挂上(docs/design/ecs-component-access-rules.md)。
    // 技能系统里对它们一律 try_get:当初把懒创建的 get_or_emplace 换成 try_get 时漏了这里的初始化,
    // 冷却从此不生效、技能上下文从不入表,施法链路整条空转。冷却不入库,重新登录从空表开始。
    const auto actor = entt::to_entity(event.actor_entity());
    if (!tlsEcs.actorRegistry.valid(actor))
    {
        LOG_ERROR << "InitializeActorComps: invalid actor entity " << event.actor_entity();
        return;
    }
    tlsEcs.actorRegistry.get_or_emplace<CooldownTimeListComp>(actor);
    tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(actor);
///<<< END WRITING YOUR CODE
}
void ActorEventHandler::InterruptCurrentStateEventHandler(const InterruptCurrentStateEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
