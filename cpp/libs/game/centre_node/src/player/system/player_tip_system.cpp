#include "player_tip_system.h"

#include "network/player_message_utils.h"
#include "service_info/centre_player_service_info.h"
#include "thread_local/redis_manager.h"
#include "thread_local/player_manager.h"


void PlayerTipSystem::SendToPlayer(entt::entity playerEntity, uint32_t tipId, const StringVector& parameters)
{
	TipInfoMessage message;
	message.set_id(tipId);
	for (auto& param : parameters)
	{
		*message.mutable_parameters()->Add() = param;
	}
	::SendMessageToClientViaGate(CentrePlayerUtilitySendTipToClientMessageId, message, playerEntity);
}

void PlayerTipSystem::SendToPlayer(Guid playerId, uint32_t tipId, const StringVector& parameters)
{
	const auto playerIterator = tlsPlayerList.find(playerId);
	if (playerIterator == tlsPlayerList.end())
	{
		return;
	}
	SendToPlayer(playerIterator->second, tipId, parameters);
}
