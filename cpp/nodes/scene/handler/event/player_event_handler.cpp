#include "player_event_handler.h"
#include "proto/logic/event/player_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "player/system/player_skill_system.h"
#include <threading/registry_manager.h>
#include <muduo/base/Logging.h>
///<<< END WRITING YOUR CODE


void PlayerEventHandler::Register()
{
    dispatcher.sink<RegisterPlayerEvent>().connect<&PlayerEventHandler::RegisterPlayerEventHandler>();
    dispatcher.sink<PlayerUpgradeEvent>().connect<&PlayerEventHandler::PlayerUpgradeEventHandler>();
    dispatcher.sink<InitializePlayerComponentsEvent>().connect<&PlayerEventHandler::InitializePlayerComponentsEventHandler>();
}

void PlayerEventHandler::UnRegister()
{
    dispatcher.sink<RegisterPlayerEvent>().disconnect<&PlayerEventHandler::RegisterPlayerEventHandler>();
    dispatcher.sink<PlayerUpgradeEvent>().disconnect<&PlayerEventHandler::PlayerUpgradeEventHandler>();
    dispatcher.sink<InitializePlayerComponentsEvent>().disconnect<&PlayerEventHandler::InitializePlayerComponentsEventHandler>();
}
void PlayerEventHandler::RegisterPlayerEventHandler(const RegisterPlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(event.actor_entity());

	if (!tlsRegistryManager.actorRegistry.valid(player))
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

	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player Not Found :" << event.actor_entity();
		return;
	}

///<<< END WRITING YOUR CODE
}
