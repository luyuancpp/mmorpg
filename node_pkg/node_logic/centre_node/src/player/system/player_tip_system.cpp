#include "player_tip_system.h"

#include "util/player_message_utils.h"
#include "service_info/centre_player_service_info.h"
#include "thread_local/storage_common_logic.h"

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
	const auto playerIterator = GlobalPlayerList().find(playerId);
	if (playerIterator == GlobalPlayerList().end())
	{
		return;
	}
	SendToPlayer(playerIterator->second, tipId, parameters);
}
