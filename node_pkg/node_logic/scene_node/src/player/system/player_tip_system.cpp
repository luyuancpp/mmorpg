#include "player_tip_system.h"

#include "core/network/message_system.h"
#include "muduo/base/Logging.h"
#include "service_info/game_client_player_service_info.h"
#include "thread_local/storage_common_logic.h"
#include "network/player_message_utils.h"
#include "thread_local/player_manager.h"

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
	const entt::entity playerEntity = PlayerManager::Instance().GetPlayer(playerId);
	if (playerEntity == entt::null)
	{
		LOG_ERROR << "Player not found for ID: " << playerId;
		return;
	}

	SendToPlayer(playerEntity, tipId, strParam);
}
