#include <memory>
#include <unordered_map>
#include "rpc/player_rpc_response_handler.h"

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
}
