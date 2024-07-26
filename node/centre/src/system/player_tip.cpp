#include "player_tip.h"

#include "network/message_system.h"
#include "service/common_client_player_service.h"
#include "thread_local/storage_common_logic.h"

void PlayerTipSystem::Tip(entt::entity player, uint32_t tip_id, const StringVector& str_param)
{
	TipS2C message;
	message.mutable_tips()->set_id(tip_id);
	for (auto& it : str_param)
	{
		*message.mutable_tips()->mutable_param()->Add() = it;
	}	
	SendToPlayer(ClientPlayerCommonServicePushTipS2CMsgId, message, player);
}

void PlayerTipSystem::Tip(Guid player_id, uint32_t tip_id, const StringVector& str_param)
{
	const auto player_it = tls_cl.player_list().find(player_id);
    if (player_it == tls_cl.player_list().end())
    {
        return;
    }
	Tip(player_it->second, tip_id, str_param);
}