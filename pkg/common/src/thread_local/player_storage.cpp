#include "player_storage.h" 

thread_local PlayerListMap gPlayerList;

entt::entity PlayerManager::GetPlayer(Guid player_uid)
{
	auto playerIt = gPlayerList.find(player_uid);
	if (gPlayerList.end() == playerIt)
	{
		return entt::null;
	}

	return playerIt->second;
}
