#include "player_tip_system.h"

#include "game_logic/network//message_util.h"
#include "service_info/player_common_service_info.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipSystem::SendToPlayer(entt::entity playerEntity, uint32_t tipId, const StringVector& parameters)
{
	TipInfoMessage message;
	message.set_id(tipId);
	for (auto& param : parameters)
	{
		*message.mutable_parameters()->Add() = param;
	}
	::SendMessageToPlayer(PlayerClientCommonServiceSendTipToClientMessageId, message, playerEntity);
}

void PlayerTipSystem::SendToPlayer(Guid playerId, uint32_t tipId, const StringVector& parameters)
{
	const auto playerIterator = tlsCommonLogic.GetPlayerList().find(playerId);
	if (playerIterator == tlsCommonLogic.GetPlayerList().end())
	{
		return;
	}
	SendToPlayer(playerIterator->second, tipId, parameters);
}
