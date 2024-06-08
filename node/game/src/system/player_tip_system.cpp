#include "player_tip_system.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "service/common_client_player_service.h"
#include "src/thread_local/game_thread_local_storage.h"
#include "src/network/message_system.h"

void PlayerTipSystem::Tip(entt::entity player, uint32_t tip_id, const StringVector& str_param)
{
	TipS2C message;
	message.mutable_tips()->set_id(tip_id);
	for (auto& it : str_param)
	{
		*message.mutable_tips()->mutable_param()->Add() = it;
	}	
	Send2Player(ClientPlayerCommonServicePushTipS2CMsgId, message, player);
}

void PlayerTipSystem::Tip(Guid player_id, uint32_t tip_id, const StringVector& str_param)
{
	auto it = game_tls.player_list().find(player_id);
	if (it == game_tls.player_list().end())
	{
		return;
	}
	Tip(it->second, tip_id, str_param);
}