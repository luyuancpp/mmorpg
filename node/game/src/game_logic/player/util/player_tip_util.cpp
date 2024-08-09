#include "player_tip_util.h"

#include "muduo/base/Logging.h"
#include "game_logic/network/message_util.h"
#include "service/common_client_player_service.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipUtil::SendToPlayer(entt::entity player, uint32_t tipId, const StringVector& strParam)
{
	TipMessage message;
	message.mutable_tip_info()->set_id(tipId);
	for (const auto& param : strParam)
	{
		*message.mutable_tip_info()->mutable_parameters()->Add() = param;
	}

	SendMessageToPlayer(ClientPlayerCommonServiceSendTipToClientMsgId, message, player);
}

void PlayerTipUtil::SendToPlayer(Guid playerId, uint32_t tipId, const StringVector& strParam)
{
	entt::entity playerEntity = tlsCommonLogic.GetPlayer(playerId);
	if (playerEntity == entt::null)
	{
		LOG_ERROR << "Player not found for ID: " << playerId;
		return;
	}

	SendToPlayer(playerEntity, tipId, strParam);
}
