#include "player_session.h"

#include "thread_local/storage.h"
#include "type_alias/player_session.h"

void PlayerSessionSystem::Initialize()
{
    tls.globalRegistry.emplace<SessionList>(global_entity());
}

