#include "player_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "player/system/player_skill.h"
#include <thread_context/registry_manager.h>
#include <muduo/base/Logging.h>
///<<< END WRITING YOUR CODE
void PlayerEventHandler::Register()
{
    tlsEcs.dispatcher.sink<RegisterPlayerEvent>().connect<&PlayerEventHandler::RegisterPlayerEventHandler>();
    tlsEcs.dispatcher.sink<PlayerUpgradeEvent>().connect<&PlayerEventHandler::PlayerUpgradeEventHandler>();
    tlsEcs.dispatcher.sink<InitializePlayerComponentsEvent>().connect<&PlayerEventHandler::InitializePlayerComponentsEventHandler>();
}

void PlayerEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<RegisterPlayerEvent>().disconnect<&PlayerEventHandler::RegisterPlayerEventHandler>();
    tlsEcs.dispatcher.sink<PlayerUpgradeEvent>().disconnect<&PlayerEventHandler::PlayerUpgradeEventHandler>();
    tlsEcs.dispatcher.sink<InitializePlayerComponentsEvent>().disconnect<&PlayerEventHandler::InitializePlayerComponentsEventHandler>();
}
void PlayerEventHandler::RegisterPlayerEventHandler(const RegisterPlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(event.actor_entity());

	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player Not Found :" << event.actor_entity();
		return;
	}

	PlayerSkillSystem::RegisterPlayer(player);
///<<< END WRITING YOUR CODE
}
void PlayerEventHandler::PlayerUpgradeEventHandler(const PlayerUpgradeEvent& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}
void PlayerEventHandler::InitializePlayerComponentsEventHandler(const InitializePlayerComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(event.actor_entity());

	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player Not Found :" << event.actor_entity();
		return;
	}

///<<< END WRITING YOUR CODE
}
