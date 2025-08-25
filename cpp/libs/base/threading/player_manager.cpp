#include "player_manager.h" 

thread_local PlayerListMap tlsPlayerList;

entt::entity GetPlayer(Guid player_uid)
{
	auto playerIt = tlsPlayerList.find(player_uid);
	if (tlsPlayerList.end() == playerIt)
	{
		return entt::null;
	}

	return playerIt->second;
}
