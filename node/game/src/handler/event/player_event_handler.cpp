#include "player_event_handler.h"
#include "logic/event/player_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_logic/player/util/player_skill_util.h"
#include "game_logic/combat/skill/util/skill_util.h"
///<<< END WRITING YOUR CODE
void PlayerEventHandler::Register()
{
		tls.dispatcher.sink<RegisterPlayer>().connect<&PlayerEventHandler::RegisterPlayerHandler>();
		tls.dispatcher.sink<PlayerUpgrade>().connect<&PlayerEventHandler::PlayerUpgradeHandler>();
		tls.dispatcher.sink<InitializePlayerComponents>().connect<&PlayerEventHandler::InitializePlayerComponentsHandler>();
}

void PlayerEventHandler::UnRegister()
{
		tls.dispatcher.sink<RegisterPlayer>().disconnect<&PlayerEventHandler::RegisterPlayerHandler>();
		tls.dispatcher.sink<PlayerUpgrade>().disconnect<&PlayerEventHandler::PlayerUpgradeHandler>();
		tls.dispatcher.sink<InitializePlayerComponents>().disconnect<&PlayerEventHandler::InitializePlayerComponentsHandler>();
}

void PlayerEventHandler::RegisterPlayerHandler(const RegisterPlayer& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(event.entity());

	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player Not Found :" << event.entity();
		return;
	}

	PlayerSkillUtil::RegisterPlayer(player);
///<<< END WRITING YOUR CODE
}

void PlayerEventHandler::PlayerUpgradeHandler(const PlayerUpgrade& event)
{
///<<< BEGIN WRITING YOUR CODE
	
///<<< END WRITING YOUR CODE
}

void PlayerEventHandler::InitializePlayerComponentsHandler(const InitializePlayerComponents& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(event.entity());

	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player Not Found :" << event.entity();
		return;
	}

	SkillUtil::InitializePlayerComponentsHandler(player);
///<<< END WRITING YOUR CODE
}

