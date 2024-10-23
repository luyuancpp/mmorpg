#include "player_event_handler.h"
#include "logic/event/player_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "component/npc_comp.pb.h"
#include "game_logic/actor/util/actor_state_attribute_sync_util.h"
#include "game_logic/combat/buff/util/buff_util.h"
#include "game_logic/player/util/player_skill_util.h"
#include "game_logic/combat/skill/util/skill_util.h"
#include "game_logic/scene/util/interest_util.h"
///<<< END WRITING YOUR CODE
void PlayerEventHandler::Register()
{
		tls.dispatcher.sink<RegisterPlayerEvent>().connect<&PlayerEventHandler::RegisterPlayerEventHandler>();
		tls.dispatcher.sink<PlayerUpgradeEvent>().connect<&PlayerEventHandler::PlayerUpgradeEventHandler>();
		tls.dispatcher.sink<InitializePlayerComponentsEvent>().connect<&PlayerEventHandler::InitializePlayerComponentsEventHandler>();
}

void PlayerEventHandler::UnRegister()
{
		tls.dispatcher.sink<RegisterPlayerEvent>().disconnect<&PlayerEventHandler::RegisterPlayerEventHandler>();
		tls.dispatcher.sink<PlayerUpgradeEvent>().disconnect<&PlayerEventHandler::PlayerUpgradeEventHandler>();
		tls.dispatcher.sink<InitializePlayerComponentsEvent>().disconnect<&PlayerEventHandler::InitializePlayerComponentsEventHandler>();
}

void PlayerEventHandler::RegisterPlayerEventHandler(const RegisterPlayerEvent& event)
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

void PlayerEventHandler::PlayerUpgradeEventHandler(const PlayerUpgradeEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	
///<<< END WRITING YOUR CODE
}

void PlayerEventHandler::InitializePlayerComponentsEventHandler(const InitializePlayerComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(event.entity());

	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player Not Found :" << event.entity();
		return;
	}
	ActorStateAttributeSyncUtil::InitializeActorComponents(player);
	SkillUtil::InitializeActorComponents(player);
	BuffUtil::InitializeActorComponents(player);
	InterestManager::InitializeActorComponents(player);
///<<< END WRITING YOUR CODE
}

