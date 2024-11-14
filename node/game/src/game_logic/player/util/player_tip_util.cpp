#include "player_tip_util.h"

#include "game_logic/core/network/message_util.h"
#include "muduo/base/Logging.h"
#include "service_info/player_common_service_info.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipUtil::SendToPlayer(entt::entity player, uint32_t tipId, const StringVector& strParam)
{
	TipInfoMessage message;
	message.set_id(tipId);
	for (const auto& param : strParam)
	{
		*message.mutable_parameters()->Add() = param;
	}

	SendMessageToPlayer(PlayerClientCommonServiceSendTipToClientMessageId, message, player);
}

void PlayerTipUtil::SendToPlayer(const Guid playerId, const uint32_t tipId, const StringVector& strParam)
{
	const entt::entity playerEntity = tlsCommonLogic.GetPlayer(playerId);
	if (playerEntity == entt::null)
	{
		LOG_ERROR << "Player not found for ID: " << playerId;
		return;
	}

	SendToPlayer(playerEntity, tipId, strParam);
}
