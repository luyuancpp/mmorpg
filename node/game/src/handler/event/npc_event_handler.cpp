#include "npc_event_handler.h"
#include "logic/event/npc_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_logic/actor/util/actor_attribute_calculator_util.h"
#include "game_logic/actor/util/actor_state_attribute_sync_util.h"
#include "game_logic/combat/buff/util/buff_util.h"
#include "game_logic/combat/skill/util/skill_util.h"
#include "game_logic/npc/util/npc_util.h"
#include "game_logic/scene/util/interest_util.h"
///<<< END WRITING YOUR CODE
void NpcEventHandler::Register()
{
		tls.dispatcher.sink<InitializeNpcComponentsEvent>().connect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}

void NpcEventHandler::UnRegister()
{
		tls.dispatcher.sink<InitializeNpcComponentsEvent>().disconnect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}

void NpcEventHandler::InitializeNpcComponentsEventHandler(const InitializeNpcComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    auto npc = entt::to_entity(event.entity());

    if (!tls.registry.valid(npc))
    {
        LOG_ERROR << "Npc Not Found :" << event.entity();
        return;
    }
    
    ActorStateAttributeSyncUtil::InitializeActorComponents(npc);
    NpcUtil::InitializeNpcComponents(npc);
    SkillUtil::InitializeActorComponents(npc);
    BuffUtil::InitializeActorComponents(npc);
    InterestUtil::InitializeActorComponents(npc);
    ActorAttributeCalculatorUtil::InitializeActorComponents(npc);
///<<< END WRITING YOUR CODE
}

