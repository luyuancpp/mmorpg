#include "player_tip.h"

#include "network/message_system.h"
#include "service/common_client_player_service.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipSystem::SendTipToPlayer(entt::entity player, uint32_t tip_id, const StringVector& str_param)
{
	TipMessage message;
	message.mutable_tip_info()->set_id(tip_id);
	for (auto& it : str_param)
	{
		*message.mutable_tip_info()->mutable_parameters()->Add() = it;
	}
	SendToPlayer(ClientPlayerCommonServicePushTipS2CMsgId, message, player);
}

void PlayerTipSystem::SendTipToPlayer(Guid player_id, uint32_t tip_id, const StringVector& str_param)
{
	const auto player_it = tls_cl.player_list().find(player_id);
	if (player_it == tls_cl.player_list().end())
	{
		return;
	}
	SendTipToPlayer(player_it->second, tip_id, str_param);
}