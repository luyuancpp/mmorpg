#include "player_tip.h"

#include "network/message_system.h"
#include "service/common_client_player_service.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipSystem::SendToPlayer(entt::entity playerEntity, uint32_t tipId, const StringVector& parameters)
{
	TipMessage message;
	message.mutable_tip_info()->set_id(tipId);
	for (auto& param : parameters)
	{
		*message.mutable_tip_info()->mutable_parameters()->Add() = param;
	}
	::SendToPlayer(ClientPlayerCommonServicePushTipS2CMsgId, message, playerEntity);
}

void PlayerTipSystem::SendToPlayer(Guid playerId, uint32_t tipId, const StringVector& parameters)
{
	const auto playerIterator = tls_cl.PlayerList().find(playerId);
	if (playerIterator == tls_cl.PlayerList().end())
	{
		return;
	}
	SendToPlayer(playerIterator->second, tipId, parameters);
}
