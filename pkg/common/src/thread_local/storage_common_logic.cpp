#include "storage_common_logic.h"

thread_local ThreadLocalStorageCommonLogic tlsCommonLogic;

entt::entity ThreadLocalStorageCommonLogic::GetPlayer(Guid player_uid)
{
    auto playerIt = playerList.find(player_uid);
    if (playerList.end() == playerIt)
    {
        return entt::null;
    }
    return playerIt->second;
}
