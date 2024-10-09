#include "npc_event_handler.h"
#include "logic/event/npc_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_logic/actor/util/actor_status_util.h"
#include "game_logic/combat/skill/util/skill_util.h"
#include "game_logic/npc/util/npc_util.h"
///<<< END WRITING YOUR CODE
void NpcEventHandler::Register()
{
		tls.dispatcher.sink<InitializeNpcComponents>().connect<&NpcEventHandler::InitializeNpcComponentsHandler>();
}

void NpcEventHandler::UnRegister()
{
		tls.dispatcher.sink<InitializeNpcComponents>().disconnect<&NpcEventHandler::InitializeNpcComponentsHandler>();
}

void NpcEventHandler::InitializeNpcComponentsHandler(const InitializeNpcComponents& event)
{
///<<< BEGIN WRITING YOUR CODE
    auto npc = entt::to_entity(event.entity());

    if (!tls.registry.valid(npc))
    {
        LOG_ERROR << "Npc Not Found :" << event.entity();
        return;
    }
    
    ActorStatusUtil::InitializeActorComponents(npc);
    SkillUtil::InitializePlayerComponents(npc);
    NpcUtil::InitializeNpcComponents(npc);
///<<< END WRITING YOUR CODE
}

