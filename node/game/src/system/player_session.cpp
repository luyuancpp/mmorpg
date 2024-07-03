#include "player_session.h"

#include "thread_local/storage.h"
#include "type_alias/player_session.h"

void PlayerSessionSystem::Init()
{
    tls.global_registry.emplace<SessionList>(global_entity());
}

