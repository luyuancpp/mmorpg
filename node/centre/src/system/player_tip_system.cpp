#include "player_tip_system.h"

#include "src/system/centre_player_system.h"
#include "src/constants/tips_id.h"
#include "src/network/message_system.h"
#include "service/common_client_player_service.h"
#include "src/thread_local/centre_thread_local_storage.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/thread_local/thread_local_storage_common_logic.h"


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
    auto player_it = cl_tls.player_list().find(player_id);
    if (player_it == cl_tls.player_list().end())
    {
        return;
    }
	Tip(player_it->second, tip_id, str_param);
}