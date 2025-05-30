#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

void InitPlayerServiceReplied()
{
}
