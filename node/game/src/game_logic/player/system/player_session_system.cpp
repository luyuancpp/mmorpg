#include "player_session_system.h"

#include "thread_local/storage.h"
#include "type_alias/player_session_type_alias.h"

void PlayerSessionSystem::Initialize()
{
    tls.globalRegistry.emplace<SessionList>(GlobalEntity());
}

