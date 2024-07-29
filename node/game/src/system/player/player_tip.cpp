#include "player_tip.h"

#include "muduo/base/Logging.h"
#include "system/network/message_system.h"
#include "service/common_client_player_service.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipSystem::SendToPlayer(entt::entity player, uint32_t tipId, const StringVector& strParam)
{
	TipMessage message;
	message.mutable_tip_info()->set_id(tipId);
	for (const auto& param : strParam)
	{
		*message.mutable_tip_info()->mutable_parameters()->Add() = param;
	}

	SendMessageToPlayer(ClientPlayerCommonServicePushTipS2CMsgId, message, player);
}

void PlayerTipSystem::SendToPlayer(Guid playerId, uint32_t tipId, const StringVector& strParam)
{
	entt::entity playerEntity = tlsCommonLogic.GetPlayer(playerId);
	if (playerEntity == entt::null)
	{
		LOG_ERROR << "Player not found for ID: " << playerId;
		return;
	}

	SendToPlayer(playerEntity, tipId, strParam);
}
