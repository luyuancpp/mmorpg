#include "player_tip.h"

#include "core/network/message_system.h"
#include "rpc/service_metadata/game_client_player_service_metadata.h"
#include "network/player_message_utils.h"
#include "thread_context/player_manager.h"

void PlayerTipSystem::SendToPlayer(entt::entity player, uint32_t tipId, const StringVector& strParam)
{
	TipInfoMessage message;
	message.set_id(tipId);
	for (const auto& param : strParam)
	{
		*message.mutable_parameters()->Add() = param;
	}

	SendMessageToClientViaGate(SceneClientPlayerCommonSendTipToClientMessageId, message, player);
}

void PlayerTipSystem::SendToPlayer(const Guid playerId, const uint32_t tipId, const StringVector& strParam)
{
	auto playerIt = tlsEcs.playerList.find(playerId);
	if (playerIt == tlsEcs.playerList.end())
	{
		return;
	}

	SendToPlayer(playerIt->second, tipId, strParam);
}

